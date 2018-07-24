// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "suobject.h"
#include "interp.h"
#include "sunumber.h"
#include "sustring.h"
#include "pack.h"
#include "sublock.h" // for persist_if_block
#include "lisp.h"
#include "sumethod.h"
#include "catstr.h"
#include "ostreamstr.h"
#include "range.h"
#include "suseq.h"
#include "gsl-lite.h"
#include <algorithm>
#include "builtinargs.h"
#include "sufunction.h"
#include "globals.h"
#include "suclass.h"
#include "eqnest.h"
using std::min;

// Object(...) function ---------------------------------------------

class MkObject : public Func {
public:
	MkObject() {
		named.num = globals("Object");
	}
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
};

Value su_object() {
	return new MkObject();
}

Value MkObject::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	if (member != CALL)
		return Func::call(self, member, nargs, nargnames, argnames, each);
	Value* args = GETSP() - nargs + 1;
	SuObject* ob;
	if (each >= 0) {
		verify(nargs == 1 && nargnames == 0);
		ob = args[0].object()->slice(each);
	} else {
		// create an object from the args
		ob = new SuObject;
		// convert args to members
		short unamed = nargs - nargnames;
		// un-named
		int i;
		for (i = 0; i < unamed; ++i)
			ob->add(args[i]);
		// named
		verify(i >= nargs || argnames);
		for (int j = 0; i < nargs; ++i, ++j)
			ob->put(symbol(argnames[j]), args[i]);
	}
	return ob;
}

//-------------------------------------------------------------------

enum Values { ITER_KEYS, ITER_VALUES, ITER_ASSOCS };

class SuObjectIter : public SuValue {
public:
	explicit SuObjectIter(
		SuObject* ob, Values v, bool iv = true, bool im = true)
		: object(ob), iter(ob->begin(iv, im)), end(ob->end()), values(v),
		  include_vec(iv), include_map(im) {
	}

	void out(Ostream& os) const override {
		os << "ObjectIter";
	}
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;

private:
	SuObject* object;
	SuObject::iterator iter;
	SuObject::iterator end;
	enum Values values;
	bool include_vec, include_map;
};

class ModificationCheck {
public:
	explicit ModificationCheck(SuObject* ob)
		: object(ob), vecsize(ob->vecsize()), mapsize(ob->mapsize()) {
	}
	~ModificationCheck() {
		if (vecsize != object->vecsize() || mapsize != object->mapsize())
			++object->version;
	}

private:
	SuObject* object;
	int vecsize;
	int mapsize;
};

SuObject::SuObject(bool ro) : readonly(ro) {
}

static int ord = ::order("Object");

int SuObject::order() const {
	return ord;
}

bool SuObject::lt(const SuValue& y) const {
	if (const SuSeq* suseq = dynamic_cast<const SuSeq*>(&y))
		return lt(*suseq->object()); // build
	int yo = y.order();
	if (yo == ord)
		return vec < static_cast<const SuObject&>(y).vec;
	else
		return ord < yo;
}

size_t SuObject::hashfn() const {
	size_t hash = hashcontrib();
	if (!vec.empty())
		hash = 31 * hash + vec[0].hashcontrib();
	if (vec.size() > 1)
		hash = 31 * hash + vec[1].hashcontrib();
	if (map.size() <= 5)
		for (auto e : map)
			hash = 31 * (31 * hash + e.val.hashcontrib()) + e.key.hashcontrib();
	return hash;
}

size_t SuObject::hashcontrib() const {
	return 31 * 31 * vec.size() + 31 * map.size();
}

SuObject::Mfn SuObject::method(Value member) {
	static Meth<SuObject> meths[] = {
		{"Size", &SuObject::Size},
		{"Add", &SuObject::Add},
		{"GetDefault", &SuObject::GetDefault},
		{"Member?", &SuObject::MemberQ},
		{"Members", &SuObject::Members},
		{"Iter", &SuObject::Iter},
		{"Copy", &SuObject::Copy},
		{"Delete", &SuObject::Delete},
		{"Find", &SuObject::Find},
		{"Assocs", &SuObject::Assocs},
		{"Base", &SuObject::Base},   // TODO remove
		{"Base?", &SuObject::BaseQ}, // TODO remove
		{"EqualRange", &SuObject::EqualRange},
		{"Erase", &SuObject::Erase},
		{"Eval", &SuObject::Eval},
		{"Eval2", &SuObject::Eval2},
		{"Join", &SuObject::Join},
		{"LowerBound", &SuObject::LowerBound},
		{"Method?", &SuObject::MethodQ},         // TODO remove
		{"MethodClass", &SuObject::MethodClass}, // TODO remove
		{"Partition!", &SuObject::Partition},
		{"Readonly?", &SuObject::ReadonlyQ},
		{"Reverse!", &SuObject::Reverse},
		{"Set_default", &SuObject::Set_default},
		{"Set_readonly", &SuObject::Set_readonly},
		{"Slice", &SuObject::Slice},
		{"Sort", &SuObject::Sort},
		{"Sort!", &SuObject::Sort},
		{"Unique!", &SuObject::Unique},
		{"UpperBound", &SuObject::UpperBound},
		{"Values", &SuObject::Values},
	};
	for (auto m : meths)
		if (m.name == member)
			return m.fn;
	return nullptr;
}

SuObject::SuObject(const SuObject& ob) : defval(ob.defval), vec(ob.vec) {
	for (auto [key, val] : ob.map)
		map[key] = val;
}

SuObject::SuObject(SuObject* ob, size_t offset)
	: defval(ob->defval),
	  vec(ob->vec.begin() + min(offset, ob->vec.size()), ob->vec.end()) {
	for (auto [key, val] : ob->map)
		map[key] = val;
}

void SuObject::add(Value x) {
	ModificationCheck mc(this);
	append(x);
}

void SuObject::append(Value x) {
	persist_if_block(x);
	vec.push_back(x);
	migrateMapToVec();
}

void SuObject::insert(int i, Value x) {
	persist_if_block(x);
	if (0 <= i && i <= vec.size()) {
		vec.insert(vec.begin() + i, x);
		migrateMapToVec();
	} else
		put(i, x);
}

void SuObject::migrateMapToVec() {
	Value num;
	while (Value* pv = map.find(num = vec.size())) {
		vec.push_back(*pv);
		map.erase(num);
	}
}

void SuObject::ck_readonly() const {
	if (readonly)
		except("can't modify readonly objects");
}

void SuObject::putdata(Value m, Value x) {
	ck_readonly();
	put(m, x);
}

void SuObject::put(Value m, Value x) {
	if (!x)
		return;
	ModificationCheck mc(this);
	persist_if_block(x);
	int i;
	if (!m.int_if_num(&i) || m != Value(i) || i < 0 || vec.size() < i)
		map[m] = x;
	else if (i == vec.size())
		add(x);
	else // if (0 <= i && i < vec.size())
		vec[i] = x;
}

static SuObject* sublist(SuObject* ob, int from, int to) {
	int size = ob->vecsize();
	SuObject* result = new SuObject();
	for (int i = from; i < to && i < size; ++i)
		result->add(ob->get(i));
	return result;
}

Value SuObject::rangeTo(int i, int j) {
	int size = vecsize();
	int f = prepFrom(i, size);
	int t = prepTo(j, size);
	return sublist(this, f, t);
}

Value SuObject::rangeLen(int i, int n) {
	int size = vecsize();
	int f = prepFrom(i, size);
	int t = f + min(n, size - f);
	return sublist(this, f, t);
}

Value SuObject::getdata(Value m) {
	return getdefault(m, defval);
}

Value SuObject::getdefault(Value m, Value def) {
	Value x;
	if ((x = get(m)))
		return x;
	if (SuObject* defval_ob = def.ob_if_ob()) {
		x = new SuObject(*defval_ob);
		if (!readonly)
			put(m, x);
		return x;
	}
	return def; // member not found
}

Value SuObject::get(Value m) const {
	int i;
	if (m.int_if_num(&i) && 0 <= i && i < vec.size())
		return vec[i];
	if (Value* pv = map.find(m))
		return *pv;
	else
		return Value();
}

// packing ----------------------------------------------------------

/*
pack format is:
	int size of vector
	list of vector values
		int size
		packed value
	int size of map
	list of map values
		member name
			int size
			packed value
		member value
			int size
			packed value

*/

static int packvalue(char* buf, Value x);
static Value unpackvalue(const char*& buf);

static char* cvt_int32(char* p, int n);
static int cvt_int32(const char* p);

const int NESTING_LIMIT = 20;

class Nest {
public:
	Nest() {
		if (nest > NESTING_LIMIT)
			except("pack: object nesting limit (" << NESTING_LIMIT
												  << ") exceeded");
		++nest;
	}
	~Nest() {
		--nest;
	}

private:
	static int nest;
};
int Nest::nest = 0;
// TODO: don't output duplicate sub-objects

size_t SuObject::packsize() const {
	int ps = 1;
	if (size() == 0)
		return ps;

	Nest nest;
	ps += sizeof(int); // vec size
	int n = vec.size();
	for (int i = 0; i < n; ++i)
		ps += sizeof(int) /* value size */ + vec[i].packsize();

	ps += sizeof(int); // map size
	for (Map::const_iterator iter = map.begin(); iter != map.end(); ++iter)
		ps += sizeof(int) /* member size */ + iter->key.packsize() +
			sizeof(int) /* value size */ + iter->val.packsize();

	return ps;
}

void SuObject::pack(char* buf) const {
	*buf++ = PACK_OBJECT;
	if (size() == 0)
		return;

	int nv = vec.size();
	cvt_int32(buf, nv);
	buf += sizeof(int);
	for (int i = 0; i < nv; ++i)
		buf += packvalue(buf, vec[i]);

	int nm = map.size();
	cvt_int32(buf, nm);
	buf += sizeof(int);
	for (Map::const_iterator iter = map.begin(); iter != map.end(); ++iter) {
		buf += packvalue(buf, iter->key); // member
		buf += packvalue(buf, iter->val); // value
	}
}

/*static*/ SuObject* SuObject::unpack(const gcstring& s) {
	return unpack2(s, new SuObject);
}

/*static*/ SuObject* SuObject::unpack2(const gcstring& s, SuObject* ob) {
	if (s.size() <= 1)
		return ob;
	const char* buf = s.ptr() + 1; // skip PACK_OBJECT

	int nv = cvt_int32(buf);
	buf += sizeof(int);
	int i;
	for (i = 0; i < nv; ++i)
		ob->add(unpackvalue(buf));

	int nm = cvt_int32(buf);
	buf += sizeof(int);
	for (i = 0; i < nm; ++i) {
		Value member = unpackvalue(buf);
		Value value = unpackvalue(buf);
		ob->put(member, value);
	}
	verify(buf == s.ptr() + s.size());
	return ob;
}

static int packvalue(char* buf, Value x) {
	int n = x.packsize();
	cvt_int32(buf, n);
	x.pack(buf + sizeof(int));
	return sizeof(int) + n;
}

static Value unpackvalue(const char*& buf) {
	int n;
	n = cvt_int32(buf);
	buf += sizeof(int);
	Value x = ::unpack(buf, n);
	buf += n;
	return x;
}

// complement leading bit to ensure correct unsigned compare

static char* cvt_int32(char* p, int n) {
	n ^= 0x80000000;
	p[3] = (char) n;
	p[2] = (char) (n >>= 8);
	p[1] = (char) (n >>= 8);
	p[0] = (char) (n >> 8);
	return p;
}

static int cvt_int32(const char* q) {
	const unsigned char* p = (unsigned char*) q;
	int n = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	return n ^ 0x80000000;
}

//-------------------------------------------------------------------

bool SuObject::erase(Value m) {
	ModificationCheck mc(this);
	int i;
	if (m.int_if_num(&i) && 0 <= i && i < vec.size()) {
		vec.erase(vec.begin() + i);
		vec.push_back(Value()); // clear to help gc
		vec.pop_back();
		return true;
	} else
		return map.erase(m);
}

// this erase does NOT shift numeric subscripts
bool SuObject::erase2(Value m) {
	ModificationCheck mc(this);
	int i;
	if (m.int_if_num(&i) && 0 <= i && i < vec.size()) {
		// migrate from vec to map
		for (int j = vec.size() - 1; j > i; --j)
			map[j] = vec[j];
		vec.erase(vec.begin() + i, vec.end());
		return true;
	}
	return map.erase(m);
}

Value SuObject::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	if (Mfn f = method(member)) {
		BuiltinArgs args(nargs, nargnames, argnames, each);
		return (this->*f)(args);
	}
	static UserDefinedMethods udm("Objects");
	if (Value c = udm(member))
		return c.call(self, member, nargs, nargnames, argnames, each);
	method_not_found(self.type(), member);
}

// Suneido methods ==================================================

Value SuObject::Set_default(BuiltinArgs& args) {
	ck_readonly();
	args.usage("object.Set_default(value) or object.Set_default()");
	defval = args.getValue("value", Value());
	args.end();
	if (SuObject* defval_ob = defval.ob_if_ob())
		if (!defval_ob->readonly)
			defval = new SuObject(*defval_ob);
	return this;
}

Value SuObject::Copy(BuiltinArgs& args) {
	args.usage("object.Copy()").end();
	return new SuObject(*this);
}

static void list_named(
	BuiltinArgs& args, bool& listq, bool& namedq, const char* usage) {
	args.usage(usage);
	Value list = args.getNamed("list");
	Value named = args.getNamed("named");
	args.end();
	if (!list && !named)
		listq = namedq = true;
	else {
		listq = list == SuTrue;
		namedq = named == SuTrue;
	}
}

Value SuObject::Size(BuiltinArgs& args) {
	bool listq, namedq;
	list_named(
		args, listq, namedq, "object.Size() or .Size(list:) or .Size(named:)");
	return (listq ? vec.size() : 0) + (namedq ? map.size() : 0);
}

Value SuObject::Iter(BuiltinArgs& args) {
	args.usage("object.Iter()").end();
	return new SuObjectIter(this, ITER_VALUES);
}

bool SuObject::has(Value mem) {
	return get(mem) != Value();
}

Value SuObject::MemberQ(BuiltinArgs& args) {
	args.usage("object.Member?(name)");
	Value mem = args.getValue("member");
	args.end();
	return has(mem) ? SuTrue : SuFalse;
}

Value SuObject::MethodQ(BuiltinArgs& args) {
	args.usage("object.Method?(name)");
	args.getValue("name");
	args.end();
	return SuFalse;
}

Value SuObject::MethodClass(BuiltinArgs& args) {
	args.usage("object.MethodClass(name)");
	args.getValue("name");
	args.end();
	return SuFalse;
}

Value SuObject::Base(BuiltinArgs& args) {
	args.usage("object.Base()").end();
	return SuFalse;
}

Value SuObject::BaseQ(BuiltinArgs& args) {
	args.usage("object.Base?(value)");
	args.getValue("value");
	args.end();
	return SuFalse;
}

Value SuObject::Eval(BuiltinArgs& args) {
	args.usage("object.Eval(function, args ...)");
	Value fn = args.getNextUnnamed();
	if (SuMethod* meth = val_cast<SuMethod*>(fn))
		fn = meth->fn();
	return args.call(fn, this, CALL);
}

Value SuObject::Eval2(BuiltinArgs& args) {
	Value result = Eval(args);
	SuObject* ob = new SuObject;
	if (result)
		ob->add(result);
	return ob;
}

struct PartVal {
	PartVal(Value v) : val(v) {
	}
	bool operator()(Value x) {
		return x < val;
	}
	Value val;
};

struct PartFn {
	PartFn(Value f) : fn(f) {
	}
	bool operator()(Value x) {
		KEEPSP
		PUSH(x);
		return SuTrue == docall(fn, CALL, 1);
	}
	Value fn;
};

Value SuObject::Partition(BuiltinArgs& args) {
	ck_readonly();
	args.usage("object.Partition(value or function)");
	Value arg = args.getValue("block");
	args.end();
	if (val_cast<Func*>(arg))
		return std::stable_partition(vec.begin(), vec.end(), PartFn(arg)) -
			vec.begin();
	else
		return std::stable_partition(vec.begin(), vec.end(), PartVal(arg)) -
			vec.begin();
}

struct Lt {
	Lt(Value f) : fn(f) {
	}
	bool operator()(Value x, Value y) {
		KEEPSP
		PUSH(x);
		PUSH(y);
		return SuTrue == docall(fn, CALL, 2);
	}
	Value fn;
};

Value SuObject::Sort(BuiltinArgs& args) {
	ck_readonly();
	args.usage("object.Sort(block = <)");
	Value fn = args.getValue("block", SuFalse);
	args.end();
	++version;
	if (fn == SuFalse)
		sort();
	else
		std::stable_sort(vec.begin(), vec.end(), Lt(fn));
	return this;
}

void SuObject::sort() {
	std::stable_sort(vec.begin(), vec.end());
}

Value SuObject::LowerBound(BuiltinArgs& args) {
	args.usage("object.LowerBound(value, block = <)");
	Value val = args.getValue("value");
	Value fn = args.getValue("block", SuFalse);
	args.end();
	if (fn == SuFalse)
		return std::lower_bound(vec.begin(), vec.end(), val) - vec.begin();
	else
		return std::lower_bound(vec.begin(), vec.end(), val, Lt(fn)) -
			vec.begin();
}

Value SuObject::UpperBound(BuiltinArgs& args) {
	args.usage("object.UpperBound(value, block = <)");
	Value val = args.getValue("value");
	Value fn = args.getValue("block", SuFalse);
	args.end();
	if (fn == SuFalse)
		return std::upper_bound(vec.begin(), vec.end(), val) - vec.begin();
	else
		return std::upper_bound(vec.begin(), vec.end(), val, Lt(fn)) -
			vec.begin();
}

Value SuObject::EqualRange(BuiltinArgs& args) {
	args.usage("object.EqualRange(value, block = <)");
	Value val = args.getValue("value");
	Value fn = args.getValue("block", SuFalse);
	args.end();
	auto pair = (fn == SuFalse)
		? std::equal_range(vec.begin(), vec.end(), val)
		: std::equal_range(vec.begin(), vec.end(), val, Lt(fn));
	SuObject* ob = new SuObject();
	ob->add(pair.first - vec.begin());
	ob->add(pair.second - vec.begin());
	return ob;
}

Value SuObject::Unique(BuiltinArgs& args) {
	// TODO allow passing equality function
	args.usage("object.Unique()").end();
	return unique();
}

Value SuObject::unique() {
	auto end = std::unique(vec.begin(), vec.end());
	vec.erase(end, vec.end());
	return this;
}

Value SuObject::Slice(BuiltinArgs& args) { // deprecated, use [...]
	args.usage("object.Slice(i, n = <size>)");
	int i = args.getint("i");
	int n = args.getint("n", vecsize());
	args.end();
	if (i < 0)
		i += vecsize();
	int j = n < 0 ? vecsize() + n : i + n;
	if (i < 0)
		i = 0;
	if (j > vecsize())
		j = vecsize();
	SuObject* ob = new SuObject();
	if (i < j) {
		vec.reserve(j - i);
		ob->vec.insert(ob->vec.end(), vec.begin() + i, vec.begin() + j);
	}
	return ob;
}

Value SuObject::Find(BuiltinArgs& args) {
	args.usage("object.Find(value)");
	Value val = args.getValue("value");
	args.end();
	return find(val);
}

Value SuObject::find(Value value) {
	for (SuObject::iterator iter = begin(); iter != end(); ++iter)
		if ((*iter).second == value)
			return (*iter).first;
	return SuFalse;
}

void SuObject::remove1(Value value) {
	Value member;
	if (SuFalse != (member = find(value)))
		erase(member);
}

Value SuObject::Delete(BuiltinArgs& args) {
	// FIXME: duplicate of SuInstance::Delete
	ck_readonly();
	args.usage("object.Delete(member ...) or Delete(all:)");
	if (Value all = args.getNamed("all")) {
		args.end();
		if (all == SuTrue)
			clear();
	} else {
		while (Value m = args.getNextUnnamed())
			erase(m);
	}
	return this;
}

void SuObject::clear() {
	ModificationCheck mc(this);
	std::fill(vec.begin(), vec.end(), Value());
	vec.clear();
	map.clear();
}

// like Delete, but doesn't move in vector
Value SuObject::Erase(BuiltinArgs& args) {
	ck_readonly();
	args.usage("object.Erase(member ...)");
	while (Value x = args.getNextUnnamed())
		erase2(x);
	args.end();
	return this;
}

Value SuObject::Add(BuiltinArgs& args) {
	ck_readonly();
	ModificationCheck mc(this);
	args.usage("object.Add(value, ... [ at: position ])");
	Value at = args.getNamed("at");
	if (!at)
		while (Value x = args.getNextUnnamed())
			append(x);
	else {
		int i;
		if (at.int_if_num(&i))
			while (Value x = args.getNextUnnamed())
				insert(i++, x);
		else {
			Value x = args.getNextUnnamed();
			args.end();
			putdata(at, x);
		}
	}
	return this;
}

Value SuObject::Reverse(BuiltinArgs& args) {
	ck_readonly();
	args.usage("object.Reverse()").end();
	++version;
	std::reverse(vec.begin(), vec.end());
	return this;
}

Value SuObject::Join(BuiltinArgs& args) {
	args.usage("object.Join(separator = '')");
	gcstring separator = args.getgcstr("separator", "");
	args.end();
	OstreamStr oss;
	for (std::vector<Value>::iterator iter = vec.begin(); iter != vec.end();) {
		if (SuString* ss = val_cast<SuString*>(*iter))
			oss << ss->gcstr();
		else
			oss << *iter;
		if (++iter == vec.end())
			break;
		oss << separator;
	}
	return new SuString(oss.gcstr());
}

Value SuObject::Set_readonly(BuiltinArgs& args) {
	args.usage("object.Set_readonly()").end();
	setReadonly();
	return this;
}

void SuObject::setReadonly() {
	if (readonly == true)
		return;
	readonly = true;
	// recurse
	for (std::vector<Value>::iterator iter = vec.begin(); iter != vec.end();
		 ++iter)
		if (SuObject* ob = val_cast<SuObject*>(*iter))
			ob->setReadonly();
	for (Map::iterator iter = map.begin(); iter != map.end(); ++iter)
		if (SuObject* ob = val_cast<SuObject*>(iter->val))
			ob->setReadonly();
}

Value SuObject::ReadonlyQ(BuiltinArgs& args) {
	args.usage("object.Readonly?()").end();
	return readonly ? SuTrue : SuFalse;
}

Value SuObject::Members(BuiltinArgs& args) {
	bool listq, namedq;
	list_named(args, listq, namedq,
		"object.Members() or .Members(list:) or .Members(named:)");
	return new SuSeq(new SuObjectIter(this, ITER_KEYS, listq, namedq));
}

Value SuObject::Values(BuiltinArgs& args) {
	bool listq, namedq;
	list_named(args, listq, namedq,
		"object.Values() or .Values(list:) or .Values(named:)");
	return new SuSeq(new SuObjectIter(this, ITER_VALUES, listq, namedq));
}

Value SuObject::Assocs(BuiltinArgs& args) {
	bool listq, namedq;
	list_named(args, listq, namedq,
		"object.Assocs() or .Assocs(list:) or .Assocs(named:)");
	return new SuSeq(new SuObjectIter(this, ITER_ASSOCS, listq, namedq));
}

Value SuObject::GetDefault(BuiltinArgs& args) { // see also: MemBase.GetDefault
	args.usage("object.GetDefault(member, default)");
	Value mem = args.getValue("member");
	Value def = args.getValue("block"); // to handle trailing block
	args.end();
	if (Value x = getdefault(mem, Value()))
		return x;
	if (0 != strcmp(def.type(), "Block"))
		return def;
	KEEPSP
	return def.call(def, CALL);
}

// ==================================================================

bool SuObject::eq(const SuValue& y) const {
	if (const SuObject* ob = const_cast<SuValue&>(y).ob_if_ob())
		return *this == *ob;
	else
		return false;
}

// prevent infinite recursion
class Track {
public:
	explicit Track(const SuObject* ob) {
		stack.push(ob);
	}
	~Track() {
		stack.pop();
	}
	static bool has(const SuObject* ob) {
		return member(stack, ob);
	}

private:
	static Lisp<const SuObject*> stack;
};
Lisp<const SuObject*> Track::stack;

void SuObject::out(Ostream& os) const {
	outdelims(os, "()");
}

bool obout_inkey = false;
struct ObOutInKey {
	ObOutInKey() {
		obout_inkey = true;
	}
	~ObOutInKey() {
		obout_inkey = false;
	}
};

void SuObject::outdelims(Ostream& os, const char* delims) const {
	if (Track::has(this)) {
		os << "...";
		return;
	}
	Track track(this);
	if (delims[0] != '[')
		os << "#";
	os << delims[0];
	int i;
	for (i = 0; i < vec.size(); ++i)
		os << (i > 0 ? ", " : "") << vec[i];

	for (auto it = map.begin(); it != map.end(); ++it) {
		if (i++ > 0)
			os << ", ";
		{
			ObOutInKey ooik; // adjust output of keys
			os << it->key;
		}
		os << ':';
		if (it->val != SuTrue)
			os << ' ' << it->val;
	}

	os << delims[1];
}

SuObject* SuObject::slice(size_t offset) {
	return new SuObject(this, offset);
}

SuObject::iterator& SuObject::iterator::operator++() {
	checkForModification();
	if (vi < vec.size())
		++vi;
	else if (mi != mend)
		++mi;
	return *this;
}

void SuObject::iterator::checkForModification() {
	if (object_version != version)
		except_err("object modified during iteration");
}

void SuObject::iterator::rewind() {
	vi = include_vec ? 0 : vec.size();
	mi = include_map ? map.begin() : map.end();
	mend = map.end();
}

SuObject::Pair SuObject::iterator::operator*() {
	if (vi < vec.size())
		return Pair(vi, vec[vi]);
	else if (mi != mend)
		return Pair(mi->key, mi->val);
	else
		return Pair(Value(), Value());
}

bool SuObject::iterator::operator==(const iterator& iter) const {
	return vi == iter.vi && mi == iter.mi;
}

int EqNest::n = 0;
const SuValue* EqNest::xs[MAX];
const SuValue* EqNest::ys[MAX];

bool SuObject::operator==(const SuObject& ob) const {
	if (this == &ob)
		return true;
	if (EqNest::has(this, &ob))
		return true;
	EqNest eqnest(this, &ob);
	return vec == ob.vec && map == ob.map;
}

// SuObjectIter -------------------------------------------------------

Value SuObjectIter::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	static Value NEXT("Next");
	static Value DUP("Dup");
	static Value INFINITE("Infinite?");

	if (member == NEXT) {
		NOARGS("objectiter.Next()");
		iter.checkForModification();
		if (iter == end)
			return this; // eof
		std::pair<Value, Value> assoc = *iter;
		if (assoc.first == Value() || assoc.second == Value())
			return this; // eof
		++iter;
		switch (values) {
		case ITER_KEYS:
			return assoc.first;
		case ITER_VALUES:
			return assoc.second;
		case ITER_ASSOCS: {
			SuObject* ob = new SuObject();
			ob->add(assoc.first);
			ob->add(assoc.second);
			return ob;
		}
		default:
			unreachable();
		}
	} else if (member == DUP) {
		return new SuObjectIter(object, values, include_vec, include_map);
	} else if (member == INFINITE)
		return SuFalse;
	else
		method_not_found(type(), member);
}

// tests ------------------------------------------------------------

#include "testing.h"
#include "random.h"

TEST(suobject_number) {
	int i;
	{
		SuObject ob;

		verify(ob.size() == 0);
		Value x = 123;
		ob.add(x);
		verify(ob.size() == 1);
		verify(ob.get(0) == x);
		verify(ob.get(SuZero) == x);

		Value s_zero = new SuString("zero");
		Value s_one = new SuString("one");
		Value s_big = new SuString("big");
		Value s_str = new SuString("string");

		// fast use
		ob.put(SuZero, s_zero);
		ob.put(1, s_one);
		ob.put("big", s_big);
		ob.put("zero", s_zero);
		verify(s_zero == ob.get(SuZero));
		verify(s_one == ob.get(1));
		verify(s_one == ob.get(new SuNumber(1)));
		verify(s_big == ob.get("big"));
		verify(s_zero == ob.get("zero"));

		// slow use
		ob.put(123456, s_big);
		ob.put("string", s_str);
		verify(s_big == ob.get(123456));
		verify(s_str == ob.get(new SuString("string")));
		verify(s_zero == ob.get(0));
		verify(s_one == ob.get(1));
		verify(s_big == ob.get("big"));
		verify(s_zero == ob.get("zero"));
	}

	// TODO: test with more random keys

	const int N = 10000;
	const int M = 10;
	SuObject ob;
	for (i = 0; i < N; ++i) {
		Value key = random(M);
		if (Value data = ob.get(key))
			ob.put(key, data + 1);
		else
			ob.put(key, 1);
	}
	int total = 0;
	for (i = 0; i < M; ++i) {
		Value data = ob.get(i);
		verify(data);
		total += data.integer();
	}
	verify(total == N);
}

TEST(suobject_empty) {
	SuObject ob;
	verify(ob.size() == 0);
	verify(!ob.get(0));
	verify(!ob.get(10));
	verify(!ob.get("x"));
}

TEST(suobject_no_match) {
	SuObject ob;
	int i;
	for (i = 0; i < 100; ++i)
		ob.put(i, i);
	char s[2] = "a";
	for (i = 0; i < 25; ++i, ++*s)
		ob.put(s, i);

	verify(!ob.get(101));
	verify(!ob.get("z"));
}

TEST(suobject_equal) {
	SuObject ob1;
	SuObject ob2;
	verify(ob1 == ob2);
	int i;
	for (i = 0; i < 100; ++i)
		ob1.put(i, i);
	verify(!(ob1 == ob2));
	for (i = 0; i < 100; ++i)
		ob2.put(i, i);
	verify(ob1 == ob2);
	char s[2] = "a";
	for (i = 0; i < 26; ++i, ++*s)
		ob1.put(s, i);

	verify(!(ob1 == ob2));
	*s = 'a';
	for (int j = 0; j < i; ++j, ++*s)
		ob2.put(s, j);
	verify(ob1 == ob2);
}

TEST(suobject_slice) {
	int i;

	// building ob1 to offset
	SuObject ob1;
	for (i = 0; i < 100; ++i)
		ob1.put(i, i);
	char s[2] = "a";
	for (i = 0; i < 26; ++i, ++*s)
		ob1.put(s, i);

	SuObject* ob = ob1.slice(2);

	// building ob2 - should be same as ob1 after slice
	SuObject ob2;
	for (i = 0; i < 98; ++i)
		ob2.put(i, i + 2);
	*s = 'a';
	for (i = 0; i < 26; ++i, ++*s)
		ob2.put(s, i);

	verify(*ob == ob2);
}

TEST(suobject_vector) {
	int i;
	SuObject ob;
	for (i = 1; i < 10; i += 2)
		ob.put(i, i);
	verify(ob.vecsize() == 0 && ob.mapsize() == 5);
	for (i = 0; i < 5; i += 2)
		ob.put(i, i);
	for (; i < 10; i += 2)
		ob.put(i, i);
	verify(ob.vecsize() == 10 && ob.mapsize() == 0);
	for (i = 0; i < 10; ++i)
		verify(ob.get(i) == i);
}

TEST(suobject_iterate) {
	int i;
	SuObject ob;
	for (i = 0; i < 100; ++i)
		ob.put(i, i);
	char c[2] = "a";
	for (i = 0; i < 26; ++i, ++*c)
		ob.put(c, i);
	verify(ob.size() == 126);

	// iterate
	SuObject::iterator iter = ob.begin();
	for (i = 0; i < 100; ++i, ++iter) {
		verify(iter != ob.end());
		verify((*iter).first == i);
		verify((*iter).second == i);
	}
	for (i = 0; i < 26; ++i, ++iter) {
		verify(iter != ob.end());
		std::pair<Value, Value> p = *iter;
		gcstring s = p.first.gcstr();
		verify(s.size() == 1 && s[0] >= 'a' && s[0] <= 'z');
		verify(p.second == s[0] - 'a');
	}
	verify(iter == ob.end());
}

TEST(suobject_remove1) {
	SuObject ob;
	ob.remove1(SuTrue);

	ob.add(SuTrue);
	assert_eq(ob.size(), 1);
	ob.remove1(SuFalse);
	assert_eq(ob.size(), 1);
	ob.remove1(SuTrue);
	assert_eq(ob.size(), 0);

	ob.add(1);
	ob.add(2);
	ob.add(4);
	ob.put(10, 2);
	ob.put(11, 3);
	assert_eq(ob.size(), 5);
	ob.remove1(2);
	assert_eq(ob.size(), 4);
	assert_eq(ob.get(0), 1);
	assert_eq(ob.get(1), 4);
	assert_eq(ob.get(10), 2);
	assert_eq(ob.get(11), 3);
}

TEST(suobject_list_named) {
	assert_eq(Value(3), run("#(1, 2, a: 3).Size()"));
	assert_eq(Value(2), run("#(1, 2, a: 3).Size(list:)"));
	assert_eq(Value(2), run("#(1, 2, a: 3).Size(@#(list:))"));
	assert_eq(Value(1), run("#(1, 2, a: 3).Size(named:)"));
	assert_eq(Value(3), run("#(1, 2, a: 3).Size(list:, named:)"));
	assert_eq(Value(2), run("#(1, 2, a: 3).Size(list:, named: false)"));
	assert_eq(Value(2), run("#(1, 2, a: 3).Size(@#(list:, named: false))"));
	assert_eq(Value(1), run("#(1, 2, a: 3).Size(list: false, named:)"));
	assert_eq(Value(0), run("#(1, 2, a: 3).Size(list: false, named: false)"));

	assert_eq(run("#(0, 1, a)"), run("#(1, 2, a: 3).Members()"));
	assert_eq(run("#(0, 1)"), run("#(1, 2, a: 3).Members(list:)"));
	assert_eq(run("#(a)"), run("#(1, 2, a: 3).Members(named:)"));
	assert_eq(run("#(0, 1, a)"), run("#(1, 2, a: 3).Members(list:, named:)"));
	assert_eq(
		run("#(0, 1)"), run("#(1, 2, a: 3).Members(list:, named: false)"));
	assert_eq(run("#(a)"), run("#(1, 2, a: 3).Members(list: false, named:)"));
	assert_eq(
		run("#()"), run("#(1, 2, a: 3).Members(list: false, named: false)"));

	assert_eq(run("#(1, 2, 3)"), run("#(1, 2, a: 3).Values()"));
	assert_eq(run("#(1, 2)"), run("#(1, 2, a: 3).Values(list:)"));
	assert_eq(run("#(3)"), run("#(1, 2, a: 3).Values(named:)"));
	assert_eq(run("#(1, 2, 3)"), run("#(1, 2, a: 3).Values(list:, named:)"));
	assert_eq(run("#(1, 2)"), run("#(1, 2, a: 3).Values(list:, named: false)"));
	assert_eq(run("#(3)"), run("#(1, 2, a: 3).Values(list: false, named:)"));
	assert_eq(
		run("#()"), run("#(1, 2, a: 3).Values(list: false, named: false)"));

	assert_eq(run("#((0, 1), (1, 2), (a, 3))"), run("#(1, 2, a: 3).Assocs()"));
	assert_eq(run("#((0, 1), (1, 2))"), run("#(1, 2, a: 3).Assocs(list:)"));
	assert_eq(run("#((a, 3))"), run("#(1, 2, a: 3).Assocs(named:)"));
	assert_eq(run("#((0, 1), (1, 2), (a, 3))"),
		run("#(1, 2, a: 3).Assocs(list:, named:)"));
	assert_eq(run("#((0, 1), (1, 2))"),
		run("#(1, 2, a: 3).Assocs(list:, named: false)"));
	assert_eq(
		run("#((a, 3))"), run("#(1, 2, a: 3).Assocs(list: false, named:)"));
	assert_eq(
		run("#()"), run("#(1, 2, a: 3).Assocs(list: false, named: false)"));
}
