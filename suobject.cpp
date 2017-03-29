/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2000 Suneido Software Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation - version 2.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License in the file COPYING
 * for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "suobject.h"
#include "interp.h"
#include "globals.h"
#include "symbols.h"
#include "sunumber.h"
#include "sustring.h"
#include "sufunction.h"
#include "func.h"
#include "suclass.h"
#include <ctype.h>
#include "cvt.h"
#include "pack.h"
#include "sublock.h" // for persist_if_block
#include "lisp.h"
#include "sumethod.h"
#include "catstr.h"
#include "ostreamstr.h"
#include "range.h"
#include "suseq.h"
#include <algorithm>
using std::min;

extern Value root_class;

enum Values { ITER_KEYS, ITER_VALUES, ITER_ASSOCS };

class SuObjectIter : public SuValue
	{
public:
	explicit SuObjectIter(SuObject* ob, Values v, bool iv = true, bool im = true)
		: object(ob), iter(ob->begin(iv, im)), end(ob->end()), values(v),
		include_vec(iv), include_map(im)
		{ }

	void out(Ostream& os) const override
		{ os << "ObjectIter"; }
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;
private:
	SuObject* object;
	SuObject::iterator iter;
	SuObject::iterator end;
	enum Values values;
	bool include_vec, include_map;
	};

class ModificationCheck
	{
public:
	explicit ModificationCheck(SuObject* ob) : object(ob), vecsize(ob->vecsize()), mapsize(ob->mapsize())
		{ }
	~ModificationCheck()
		{
		if (vecsize != object->vecsize() || mapsize != object->mapsize())
			++object->version;
		}
private:
	SuObject* object;
	int vecsize;
	int mapsize;
	};

SuObject::SuObject()
	: myclass(root_class), readonly(false), has_getter(true), has_setter(true), version(0)
	{
	init();
	}

SuObject::SuObject(bool ro)
	: myclass(root_class), readonly(ro), has_getter(true), has_setter(true), version(0)
	{
	init();
	}

void SuObject::init()
	{
	static bool first = true;
	if (first)
		{
		setup();
		first = false;
		}
	}

static int ord = ::order("Object");

int SuObject::order() const
	{ return ord; }

bool SuObject::lt(const SuValue& y) const
	{
	if (const SuSeq* suseq = dynamic_cast<const SuSeq*>(&y))
		return lt(*suseq->object()); // build
	int yo = y.order();
	if (yo == ord)
		return vec < static_cast<const SuObject&>(y).vec;
	else
		return ord < yo;
	}

static int hash_nest = 0;
struct HashCount
	{
	HashCount()
		{ ++hash_nest; }
	~HashCount()
		{ --hash_nest; }
	};

size_t SuObject::hashfn() const
	{
	HashCount nest;
	size_t hash = size();
	if (hash_nest > 4)
		return hash;
	for (int i = 0; i < vec.size(); ++i)
		hash += vec[i].hash();
	for (auto it = map.begin(); it != map.end(); ++it)
		hash += it->val.hash() + it->key.hash();
	return hash;
	}

/** methods are available to Objects (not instances or classes) */
HashMap<Value,SuObject::pmfn> SuObject::methods;

/** basic_methods are available to Objects, instances, and classes */
HashMap<Value,SuObject::pmfn> SuObject::basic_methods;

/** instance_methods are available to Objects and instances (not classes) */
HashMap<Value, SuObject::pmfn> SuObject::instance_methods;

#define METHOD(fn) methods[#fn] = &SuObject::fn
#define BASIC_METHOD(fn) basic_methods[#fn] = &SuObject::fn
#define INSTANCE_METHOD(fn) instance_methods[#fn] = &SuObject::fn

void SuObject::setup()
	{
	BASIC_METHOD(Base);
	basic_methods["Base?"] = &SuObject::HasBase;
	BASIC_METHOD(Eval);
	BASIC_METHOD(Eval2);
	BASIC_METHOD(Find);
	BASIC_METHOD(GetDefault);
	BASIC_METHOD(Iter);
	BASIC_METHOD(Join);
	basic_methods["Member?"] = &SuObject::HasMember;
	basic_methods["Method?"] = &SuObject::HasMethod;
	BASIC_METHOD(Members);
	BASIC_METHOD(MethodClass);
	basic_methods["Readonly?"] = &SuObject::IsReadonly;
	BASIC_METHOD(Size);

	METHOD(Add);
	METHOD(Assocs);
	METHOD(Erase);
	METHOD(EqualRange);
	METHOD(LowerBound);
	methods["Reverse!"] = &SuObject::Reverse;
	METHOD(Set_default);
	METHOD(Set_readonly);
	METHOD(Slice);
	methods["Partition!"] = &SuObject::Partition;
	METHOD(Sort);
	methods["Sort!"] = &SuObject::Sort;
	methods["Unique!"] = &SuObject::Unique;
	METHOD(UpperBound);
	METHOD(Values);

	INSTANCE_METHOD(Copy);
	INSTANCE_METHOD(Delete);
	}

SuObject::SuObject(const SuObject& ob) : myclass(ob.myclass), defval(ob.defval),
	vec(ob.vec), readonly(false), has_getter(true), has_setter(true), version(0)
	{
	for (Map::const_iterator it = ob.map.begin(), end = ob.map.end(); it != end; ++it)
		map[it->key] = it->val;
	}

SuObject::SuObject(SuObject* ob, size_t offset)
	: myclass(ob->myclass), defval(ob->defval),
	vec(ob->vec.begin() + min(offset, ob->vec.size()), ob->vec.end()),
	readonly(false), has_getter(true), has_setter(true), version(0)
	{
	for (Map::iterator it = ob->map.begin(), end = ob->map.end(); it != end; ++it)
		map[it->key] = it->val;
	}

void SuObject::add(Value x)
	{
	ModificationCheck mc(this);
	persist_if_block(x);
	vec.push_back(x);
	// check for migration from map to vec
	Value num;
	while (Value* pv = map.find(num = vec.size()))
		{
		vec.push_back(*pv);
		map.erase(num);
		}
	}

void SuObject::putdata(Value m, Value x)
	{
	if (readonly)
		except("can't modify readonly objects");
	if (has_setter && ! has(m))
		{
		static Value Set_("Set_");
		if (Value method = myclass.getdata(Set_))
			{
			KEEPSP
			PUSH(m);
			PUSH(x);
			method.call(this, CALL, 2, 0, 0, -1);
			return ;
			}
		else
			has_setter = false; // avoid future attempts
		}
	put(m, x);
	}

void SuObject::put(Value m, Value x)
	{
	if (! x)
		return ;
	ModificationCheck mc(this);
	persist_if_block(x);
	int i;
	if (! m.int_if_num(&i) || m != Value(i) || i < 0 || vec.size() < i)
		map[m] = x;
	else if (i == vec.size())
		add(x);
	else // if (0 <= i && i < vec.size())
		vec[i] = x;
	}

Value SuObject::getdata(Value member)
	{
	if (Range* r = val_cast<Range*>(member))
		return r->sublist(this);
	else
		return getdefault(member, defval);
	}

Value SuObject::getdefault(Value member, Value def)
	{
	Value x;
	if ((x = get(member)))
		return x;
	if ((x = myclass.getdata(member)))
		{
		if (SuFunction* sufn = val_cast<SuFunction*>(x))
			return new SuMethod(this, member, sufn);
		return x;
		}
	if (has_getter)
		{
		static Value Get_("Get_");
		if (Value method = myclass.getdata(Get_))
			{
			KEEPSP
			PUSH(member);
			return method.call(this, CALL, 1, 0, 0, -1);
			}
		else
			has_getter = false; // avoid future attempts
		}
	if (!myclass.sameAs(root_class.ptr()))
		if (const char* s = member.str_if_str())
			{
			Value getter = new SuString(CATSTRA(islower(*s) ? "get_" : "Get_", s));
			if (Value method = myclass.getdata(getter))
				{
				KEEPSP
				return method.call(this, CALL, 0, 0, 0, -1);
				}
			}
	if (SuObject* defval_ob = def.ob_if_ob())
		{
		x = new SuObject(*defval_ob);
		if (! readonly)
			put(member, x);
		return x;
		}
	return def; // member not found
	}

Value SuObject::get(Value m) const
	{
	int i;
	if (m.int_if_num(&i) && 0 <= i && i < vec.size())
		return vec[i];
	Value* pv = map.find(m);
	if (pv)
		return *pv;
	else
		return Value();
	}

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

const int NESTING_LIMIT = 20;

class Nest
	{
public:
	Nest()
		{
		if (nest > NESTING_LIMIT)
			except("pack: object nesting limit (" << NESTING_LIMIT << ") exceeded");
		++nest;
		}
	~Nest()
		{ --nest; }
private:
	static int nest;
	};
int Nest::nest = 0;
// TODO: don't output duplicate sub-objects

size_t SuObject::packsize() const
	{
	if (myclass != root_class)
		except("can't pack class instance");
	int ps = 1;
	if (size() == 0)
		return ps;

	Nest nest;
	ps += sizeof (long); // vec size
	int n = vec.size();
	for (int i = 0; i < n; ++i)
		ps += sizeof (long) /* value size */ + vec[i].packsize();

	ps += sizeof (long); // map size
	for (Map::const_iterator iter = map.begin(); iter != map.end(); ++iter)
		ps += sizeof (long) /* member size */ + iter->key.packsize() +
			sizeof (long) /* value size */ + iter->val.packsize();

	return ps;
	}

void SuObject::pack(char* buf) const
	{
	if (myclass != root_class)
		except("can't pack class instance");
	*buf++ = PACK_OBJECT;
	if (size() == 0)
		return ;

	int nv = vec.size();
	cvt_long(buf, nv);
	buf += sizeof (long);
	for (int i = 0; i < nv; ++i)
		buf += packvalue(buf, vec[i]);

	int nm = map.size();
	cvt_long(buf, nm);
	buf += sizeof (long);
	for (Map::const_iterator iter = map.begin(); iter != map.end(); ++iter)
		{
		buf += packvalue(buf, iter->key); // member
		buf += packvalue(buf, iter->val); // value
		}
	}

/*static*/ SuObject* SuObject::unpack(const gcstring& s)
	{
	return unpack2(s, new SuObject);
	}

/*static*/ SuObject* SuObject::unpack2(const gcstring& s, SuObject* ob)
	{
	if (s.size() <= 1)
		return ob;
	const char* buf = s.buf() + 1; // skip PACK_OBJECT

	int nv = cvt_long(buf);
	buf += sizeof (long);
	int i;
	for (i = 0; i < nv; ++i)
		ob->add(unpackvalue(buf));

	int nm = cvt_long(buf);
	buf += sizeof (long);
	for (i = 0; i < nm; ++i)
		{
		Value member = unpackvalue(buf);
		Value value = unpackvalue(buf);
		ob->put(member, value);
		}
	verify(buf == s.end());
	return ob;
	}

bool SuObject::erase(Value m)
	{
	ModificationCheck mc(this);
	int i;
	if (m.int_if_num(&i) && 0 <= i && i < vec.size())
		{
		vec.erase(vec.begin() + i);
		vec.push_back(Value()); // clear to help gc
		vec.pop_back();
		return true;
		}
	else
		return map.erase(m);
	}

// this erase does NOT shift numeric subscripts
bool SuObject::erase2(Value m)
	{
	ModificationCheck mc(this);
	int i;
	if (m.int_if_num(&i) && 0 <= i && i < vec.size())
		{
		// migrate from vec to map
		for (int j = vec.size() - 1; j > i; --j)
			map[j] = vec[j];
		vec.erase(vec.begin() + i, vec.end());
		return true;
		}
	return map.erase(m);
	}

/** used for both Objects and instances */
Value SuObject::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	if (member == CALL)
		member = CALL_INSTANCE;
	if (tls().proc->super)
		{
		short super = tls().proc->super; tls().proc->super = 0;
		return globals[super].call(self, member, nargs, nargnames, argnames, each);
		}
	if (pmfn* p = basic_methods.find(member))
		return (this->*(*p))(nargs, nargnames, argnames, each);
	if (pmfn* p = instance_methods.find(member))
		return (this->*(*p))(nargs, nargnames, argnames, each);
	if (myclass.sameAs(root_class)) // Object (not instance)
		if (pmfn* p = methods.find(member))
			return (this->*(*p))(nargs, nargnames, argnames, each);
	return myclass.call(self, member, nargs, nargnames, argnames, each);
	}

// suneido methods =======================================================

Value SuObject::Set_default(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0 && nargs != 1)
		except("usage: object.Set_default(value) or object.Set_default()");
	if (readonly)
		except("can't set_default on readonly object");
	defval = nargs == 0 ? Value() : TOP();
	if (SuObject* defval_ob = defval.ob_if_ob())
		if (! defval_ob->readonly)
			defval = new SuObject(*defval_ob);
	return this;
	}

Value SuObject::Copy(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: object.Copy()");
	return new SuObject(*this);
	}

static void list_named(short nargs, short nargnames, ushort* argnames,
	bool& listq, bool& namedq, const char* usage)
	{
	if (nargs > nargnames || nargs > 2)
		except(usage);
	static ushort list = ::symnum("list");
	static ushort named = ::symnum("named");
	listq = namedq = (nargs == 0 || ! (ARG(0) == SuTrue));
	for (int i = 0; i < nargs && i < nargnames; ++i)
		if (argnames[i] == list)
			listq = (ARG(i) == SuTrue);
		else if (argnames[i] == named)
			namedq = (ARG(i) == SuTrue);
		else
			except(usage);
	}

Value SuObject::Size(short nargs, short nargnames, ushort* argnames, int each)
	{
	bool listq, namedq;
	list_named(nargs, nargnames, argnames, listq, namedq,
		"usage: object.Size() or .Size(list:) or .Size(named:)");
	return (listq ? vec.size() : 0) + (namedq ? map.size() : 0);
	}

Value SuObject::Iter(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: object.Iter()");
	return new SuObjectIter(this, ITER_VALUES);
	}

template <class Finder>
Value lookup(SuObject* ob, Finder finder)
	{
	SuClass* c = dynamic_cast<SuClass*>(ob);
	if (! c)
		{
		if (! ob->myclass)
			return Value();
		c = val_cast<SuClass*>(ob->myclass);
		}
	while (c)
		{
		if (Value x = finder(c))
			return x;
		if (c->base < 0)
			break ;
		c = val_cast<SuClass*>(globals[c->base]);
		}
	return Value();
	}

struct MemberFinder
	{
	MemberFinder(Value m) : member(m)
		{ }
	Value operator()(const SuClass* c) const
		{ return c->get(member); }
	Value member;
	};

bool SuObject::has(Value mem)
	{
	return get(mem) || lookup(this, MemberFinder(mem));
	}

Value SuObject::HasMember(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1)
		except("usage: object.Member?(name)");
	Value mem = TOP();
	return has(mem) ? SuTrue : SuFalse;
	}

struct MethodFinder
	{
	MethodFinder(Value m) : method(m)
		{ }
	Value operator()(SuClass* c)
		{
		if (Value x = c->get(method))
			return val_cast<SuFunction*>(x) ? Value(c) : SuFalse;
		return Value();
		}
	Value method;
	};

Value SuObject::HasMethod(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1)
		except("usage: object.Method?(name)");
	return hasMethod(TOP()) ? SuTrue : SuFalse;
	}

bool SuObject::hasMethod(Value name)
	{
	Value x = lookup(this, MethodFinder(name));
	return x && x != SuFalse;
	}

Value SuObject::MethodClass(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1)
		except("usage: object.MethodClass(name)");
	Value x = lookup(this, MethodFinder(TOP()));
	return x ? x : SuFalse;
	}

// TODO: split Base and Base? into SuClass and SuObject
Value SuObject::Base(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: object.Base()");
	if (SuClass* c = dynamic_cast<SuClass*>(this))
		return globals[c->base];
	else
		return myclass;
	}

struct BaseFinder
	{
	BaseFinder(Value b) : base(b)
		{ }
	Value operator()(SuClass* c)
		{ return c == base.ptr() ? Value(c) : Value(); }
	Value base;
	};

Value SuObject::HasBase(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1)
		except("usage: object.Base?(class)");
	if (myclass == TOP())
		return SuTrue; // handle Object().Base?(Object)
	return lookup(this, BaseFinder(TOP())) ? SuTrue : SuFalse;
	}

Value SuObject::Eval(short nargs, short nargnames, ushort* argnames, int each)
	{
	argseach(nargs, nargnames, argnames, each);
	if (nargs < 1)
		except("usage: object.Eval(function, args ...)");
	Value x = ARG(0); // the function to call
	if (SuMethod* meth = val_cast<SuMethod*>(x))
		x = meth->fn();
	return x.call(this, CALL, nargs - 1, nargnames, argnames, each);
	}

Value SuObject::Eval2(short nargs, short nargnames, ushort* argnames, int each)
	{
	Value result = Eval(nargs, nargnames, argnames, each);
	SuObject* ob = new SuObject;
	if (result)
		ob->add(result);
	return ob;
	}

struct PartVal
	{
	PartVal(Value v) : val(v)
		{ }
	bool operator()(Value x)
		{ return x < val; }
	Value val;
	};

struct PartFn
	{
	PartFn(Value f) : fn(f)
		{ }
	bool operator()(Value x)
		{
		KEEPSP
		PUSH(x);
		return SuTrue == docall(fn, CALL, 1, 0, 0, -1);
		}
	Value fn;
	};

Value SuObject::Partition(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (readonly)
		except("can't partition readonly objects");
	if (nargs != 1)
		except("usage: object.Partition(number or string or function)");
	Value arg = ARG(0);
	if (arg.is_int() || val_cast<SuNumber*>(arg) || val_cast<SuString*>(arg))
		return std::stable_partition(vec.begin(), vec.end(), PartVal(arg)) - vec.begin();
	else
		return std::stable_partition(vec.begin(), vec.end(), PartFn(arg)) - vec.begin();
	}

struct Lt
	{
	Lt(Value f) : fn(f)
		{ }
	bool operator()(Value x, Value y)
		{
		KEEPSP
		PUSH(x);
		PUSH(y);
		return SuTrue == docall(fn, CALL, 2, 0, 0, -1);
		}
	Value fn;
	};

Value SuObject::Sort(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (readonly)
		except("can't sort readonly objects");
	++version;
	if (nargs == 0 || (nargs == 1 && ARG(0) == SuFalse))
		sort();
	else if (nargs == 1)
		{
		Value fn = ARG(0);
		std::stable_sort(vec.begin(), vec.end(), Lt(fn));
		}
	else
		except("usage: object.Sort(less_function = false)");
	return this;
	}

void SuObject::sort()
	{
	std::stable_sort(vec.begin(), vec.end());
	}

Value SuObject::LowerBound(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs == 1)
		return std::lower_bound(vec.begin(), vec.end(), ARG(0)) - vec.begin();
	else if (nargs == 2)
		return std::lower_bound(vec.begin(), vec.end(), ARG(0), Lt(ARG(1))) - vec.begin();
	else
		except("usage: object.LowerBound(value) or object.LowerBound(value, less_function)");
	}

Value SuObject::UpperBound(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs == 1)
		return std::upper_bound(vec.begin(), vec.end(), ARG(0)) - vec.begin();
	else if (nargs == 2)
		return std::upper_bound(vec.begin(), vec.end(), ARG(0), Lt(ARG(1))) - vec.begin();
	else
		except("usage: object.UpperBound(value) or object.UpperBound(value, less_function)");
	}

Value SuObject::EqualRange(short nargs, short nargnames, ushort* argnames, int each)
	{
	std::pair<Vector::iterator,Vector::iterator> pair;
	if (nargs == 1)
		pair = std::equal_range(vec.begin(), vec.end(), ARG(0));
	else if (nargs == 2)
		pair = std::equal_range(vec.begin(), vec.end(), ARG(0), Lt(ARG(1)));
	else
		except("usage: object.EqualRange(value) or object.EqualRange(value, less_function)");
	SuObject* ob = new SuObject();
	ob->add(pair.first - vec.begin());
	ob->add(pair.second - vec.begin());
	return ob;
	}

Value SuObject::Unique(short nargs, short nargnames, ushort* argnames, int each)
	{
	// TODO allow passing equality function
	return unique();
	}

Value SuObject::unique()
	{
	std::vector<Value>::iterator end = std::unique(vec.begin(), vec.end());
	vec.erase(end, vec.end());
	return this;
	}

Value SuObject::Slice(short nargs, short nargnames, ushort* argnames, int each)
	{
	argseach(nargs, nargnames, argnames, each);
	int i = nargs < 1 ? 0 : ARG(0).integer();
	int n = nargs < 2 ? vecsize() : ARG(1).integer();
	if (i < 0)
		i += vecsize();
	int j = n < 0 ? vecsize() + n : i + n;
	if (i < 0)
		i = 0;
	if (j > vecsize())
		j = vecsize();
	SuObject* ob = new SuObject();
	if (i < j)
		{
		vec.reserve(j - i);
		ob->vec.insert(ob->vec.end(), vec.begin() + i, vec.begin() + j);
		}
	return ob;
	}

Value SuObject::Find(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 1)
		except("usage: object.Find(value)");

	return find(ARG(0));
	}

Value SuObject::find(Value value)
	{
	for (SuObject::iterator iter = begin(); iter != end(); ++iter)
		if ((*iter).second == value)
			return (*iter).first;
	return SuFalse;
	}

void SuObject::remove1(Value value)
	{
	Value member;
	if (SuFalse != (member = find(value)))
		erase(member);
	}

Value SuObject::Delete(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (readonly)
		except("can't Delete from readonly objects");
	argseach(nargs, nargnames, argnames, each);
	static ushort all = ::symnum("all");
	if (nargs == 1 && nargnames == 1 && argnames[0] == all)
		{
		if (TOP() == SuTrue)
			clear();
		}
	else if (nargs > 0 && nargnames == 0)
		{
		for (int i = 0; i < nargs; ++i)
			erase(ARG(i));
		}
	else
		except("usage: object.Delete(member ...) or Delete(all:)");
	return this;
	}

void SuObject::clear()
	{
	ModificationCheck mc(this);
	std::fill(vec.begin(), vec.end(), Value());
	vec.clear();
	map.clear();
	}

// like Delete, but doesn't move in vector
Value SuObject::Erase(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (readonly)
		except("can't Erase from readonly objects");
	argseach(nargs, nargnames, argnames, each);
	if (nargs > 0 && nargnames == 0)
		for (int i = 0; i < nargs; ++i)
			erase2(ARG(i));
	else
		except("usage: object.Erase(member ...)");
	return this;
	}

Value SuObject::Add(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (readonly)
		except("can't Add to readonly objects");
	ModificationCheck mc(this);
	// optimize Add(@ob) where this and ob have only vector
	if (each >= 0 && mapsize() == 0)
		{
		verify(nargs == 1);
		verify(nargnames == 0);
		SuObject* ob = ARG(0).object();
		if (ob->mapsize() == 0)
			{
			if (each < ob->vecsize())
				vec.insert(vec.end(), ob->vec.begin() + each, ob->vec.end());
			return this;
			}
		}

	argseach(nargs, nargnames, argnames, each);
	static ushort at = ::symnum("at");
	if (nargnames > 1 || (nargnames == 1 && argnames[0] != at))
		except("usage: object.Add(value, ... [ at: position ])");
	int i = INT_MAX;
	if (nargnames)
		ARG(nargs - 1).int_if_num(&i);
	else
		i = vec.size();
	if (0 <= i && i <= vec.size())
		{
		for (int j = 0; j < nargs - nargnames; ++j, ++i)
			{
			persist_if_block(ARG(j));
			vec.insert(vec.begin() + i, ARG(j));
			// migrate from map to vector if necessary
			Value num;
			while (Value* pv = map.find(num = vec.size()))
				{
				vec.push_back(*pv);
				map.erase(num);
				}
			}
		}
	else if (nargnames == 1 && nargs == 2)
		putdata(ARG(1), ARG(0));
	else if (i < INT_MAX)
		for (int j = 0; j < nargs - 1; ++j, ++i)
			putdata(i, ARG(j));
	else
		except("can only Add multiple values to un-named or to numeric positions");
	return this;
	}

Value SuObject::Reverse(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: object.Reverse()");
	if (readonly)
		except("can't Reverse readonly objects");
	++version;
	std::reverse(vec.begin(), vec.end());
	return this;
	}

Value SuObject::Join(short nargs, short nargnames, ushort* argnames, int each)
	{
	gcstring separator;
	if (nargs == 1)
		separator = ARG(0).gcstr();
	else if (nargs != 0)
		except("usage: object.Join(separator = '')");
	OstreamStr oss;
	for (std::vector<Value>::iterator iter = vec.begin(); iter != vec.end(); )
		{
		if (SuString* ss = val_cast<SuString*>(*iter))
			oss << ss->gcstr();
		else
			oss << *iter;
		if (++iter == vec.end())
			break ;
		oss << separator;
		}
	return new SuString(oss.size(), oss.str());
	}

Value SuObject::Set_readonly(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: object.Set_readonly()");
	setReadonly();
	return this;
	}

void SuObject::setReadonly()
	{
	if (readonly == true)
		return ;
	readonly = true;
	// recurse
	for (std::vector<Value>::iterator iter = vec.begin(); iter != vec.end(); ++iter)
		if (SuObject* ob = val_cast<SuObject*>(*iter))
			ob->setReadonly();
	for (Map::iterator iter = map.begin(); iter != map.end(); ++iter)
		if (SuObject* ob = val_cast<SuObject*>(iter->val))
			ob->setReadonly();
	}

Value SuObject::IsReadonly(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 0)
		except("usage: object.Readonly?()");
	return readonly ? SuTrue : SuFalse;
	}

// returns true if ob is a class or an instance
static bool classy(SuObject* ob)
	{
	return dynamic_cast<SuClass*>(ob) || val_cast<SuClass*>(ob->myclass);
	}

static SuClass* base(SuObject* ob)
	{
	if (SuClass* c = dynamic_cast<SuClass*>(ob))
		return val_cast<SuClass*>(globals[c->base]);
	else
		return val_cast<SuClass*>(ob->myclass);
	}

void SuObject::addMembers(SuObject* list)
	{
	for (auto i = map.begin(); i != map.end(); ++i)
		list->add(i->key);
	}

Value SuObject::Members(short nargs, short nargnames, ushort* argnames, int each)
	{
	argseach(nargs, nargnames, argnames, each);
	static ushort all = ::symnum("all");
	if (nargs == 1 && nargnames == 1 && argnames[0] == all && classy(this))
		{
		SuObject* mems = new SuObject();
		SuObject* ob = this;
		do
			{
			ob->addMembers(mems);
			ob = base(ob);
			}
			while (ob);
		mems->sort();
		mems->unique();
		return mems;
		}
	bool listq, namedq;
	list_named(nargs, nargnames, argnames, listq, namedq,
		"usage: object.Members() or .Members(list: or named: or all:)");
	return new SuSeq(new SuObjectIter(this, ITER_KEYS, listq, namedq));
	}

Value SuObject::Values(short nargs, short nargnames, ushort* argnames, int each)
	{
	argseach(nargs, nargnames, argnames, each);
	bool listq, namedq;
	list_named(nargs, nargnames, argnames, listq, namedq,
		"usage: object.Values() or .Values(list:) or .Values(named:)");
	return new SuSeq(new SuObjectIter(this, ITER_VALUES, listq, namedq));
	}

Value SuObject::Assocs(short nargs, short nargnames, ushort* argnames, int each)
	{
	argseach(nargs, nargnames, argnames, each);
	bool listq, namedq;
	list_named(nargs, nargnames, argnames, listq, namedq,
		"usage: object.Assocs() or .Assocs(list:) or .Assocs(named:)");
	return new SuSeq(new SuObjectIter(this, ITER_ASSOCS, listq, namedq));
	}

Value SuObject::GetDefault(short nargs, short nargnames, ushort* argnames, int each)
	{
	if (nargs != 2)
		except("usage: object.GetDefault(member, default)");
	Value def = ARG(1);
	Value x = getdefault(ARG(0), Value());
	if (x)
		return x;
	if (0 != strcmp(def.type(), "Block"))
		return def;
	KEEPSP
	return def.call(def, CALL, 0, 0, 0, -1);
	}

// ==================================================================

bool SuObject::eq(const SuValue& y) const
	{
	if (const SuObject* ob = dynamic_cast<const SuObject*>(&y))
		return *this == *ob;
	else if (const SuSeq* seq = dynamic_cast<const SuSeq*>(&y))
		return *this == *seq->object();
	else
		return false;
	}

// prevent infinite recursion
class Track
	{
	public:
		explicit Track(const SuObject* ob)
			{ stack.push(ob); }
		~Track()
			{ stack.pop(); }
		static bool has(const SuObject* ob)
			{ return member(stack, ob); }
	private:
		static Lisp<const SuObject*> stack;
	};
Lisp<const SuObject*> Track::stack;

void SuObject::out(Ostream& os) const
	{
	outdelims(os, "()");
	}

bool obout_inkey = false;
struct ObOutInKey
	{
	ObOutInKey()
		{ obout_inkey = true; }
	~ObOutInKey()
		{ obout_inkey = false; }
	};

void SuObject::outdelims(Ostream& os, const char* delims) const
	{
	static Value ToString("ToString");

	Value c = lookup(const_cast<SuObject*>(this), MethodFinder(ToString));
	if (c && c != SuFalse)
		{
		Value x = c.call(const_cast<SuObject*>(this), ToString, 0, 0, 0, -1);
		if (! x)
			except("ToString must return a value");
		if (const char* s = x.str_if_str())
			os << s;
		else
			os << x;
		return ;
		}

	if (auto n = myclass.get_named())
		{
		os << n->name() << "()";
		return ;
		}
	if (Track::has(this))
		{
		os << "...";
		return ;
		}
	Track track(this);
	if (delims[0] != '[')
		os << "#";
	if (myclass && !myclass.sameAs(root_class))
		os << "/*" << myclass << "*/";
	os << delims[0];
	int i;
	for (i = 0; i < vec.size(); ++i)
		os << (i > 0 ? ", " : "") << vec[i];

	for (auto it = map.begin(); it != map.end(); ++it)
		{
		if (i++ > 0)
			os << ", ";
		{
		ObOutInKey ooik; // adjust output of keys
		os << it->key;
		}
		os << ": " << it->val;
		}

	os << delims[1];
	}

SuObject* SuObject::slice(size_t offset)
	{
	return new SuObject(this, offset);
	}

SuObject::iterator& SuObject::iterator::operator++()
	{
	checkForModification();
	if (vi < vec.size())
		++vi;
	else if (mi != mend)
		++mi;
	return *this;
	}

void SuObject::iterator::checkForModification()
	{
	if (object_version != version)
		except_err("object modified during iteration");
	}

void SuObject::iterator::rewind()
	{
	vi = include_vec ? 0 : vec.size();
	mi = include_map ? map.begin() : map.end();
	mend = map.end();
	}

SuObject::Pair SuObject::iterator::operator*()
	{
	if (vi < vec.size())
		return Pair(vi, vec[vi]);
	else if (mi != mend)
		return Pair(mi->key, mi->val);
	else
		return Pair(Value(), Value());
	}

bool SuObject::iterator::operator==(const iterator& iter) const
	{
	return vi == iter.vi && mi == iter.mi;
	}

// catch self reference
struct EqNest
	{
	EqNest(const SuObject* x, const SuObject* y)
		{
		if(n >= MAX)
			except("object comparison nesting overflow");
		xs[n] = x;
		ys[n] = y;
		++n;
		}
	static bool has(const SuObject* x, const SuObject* y)
		{
		for (int i = 0; i < n; ++i)
			if ((xs[i] == x && ys[i] == y) || (xs[i] == y && ys[i] == x))
				return true;
		return false;
		}
	~EqNest()
		{ --n; verify(n >= 0); }
	enum { MAX = 20 };
	static int n;
	static const SuObject* xs[MAX];
	static const SuObject* ys[MAX];
	};
int EqNest::n = 0;
const SuObject* EqNest::xs[MAX];
const SuObject* EqNest::ys[MAX];

typedef SuObject::Vector Vec;

typedef SuObject::Map Map;

static bool mapeq(const Map& m1, const Map& m2)
	{
	if (m1.size() != m2.size())
		return false;
	for (Map::const_iterator it = m1.begin(); it != m2.end(); ++it)
		{
		Value* pv = m2.find(it->key);
		if (! pv || *pv != it->val)
			return false;
		}
	return true;
	};

bool SuObject::operator==(const SuObject& ob) const
	{
	if (this == &ob)
		return true;
	if (EqNest::has(this, &ob))
		return true;
	EqNest eqnest(this, &ob);
	return myclass == ob.myclass && vec == ob.vec && mapeq(map, ob.map);
	}

void SuObject::set_members(SuObject* ob)
	{
	ModificationCheck mc(this);
	vec = ob->vec;
	map = Map();
	for (Map::iterator it = ob->map.begin(), end = ob->map.end(); it != end; ++it)
		map[it->key] = it->val;
	}

// SuObjectIter -------------------------------------------------------

Value SuObjectIter::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value NEXT("Next");
	static Value ITER("Iter");
	static Value COPY("Copy");
	static Value REWIND("Rewind");

	if (member == NEXT)
		{
		if (nargs != 0)
			except("usage: objectiter.Next()");
		iter.checkForModification();
		if (iter == end)
			return this; // eof
		std::pair<Value,Value> assoc = *iter;
		if (assoc.first == Value() || assoc.second == Value())
			return this; // eof
		++iter;
		switch (values)
			{
		case ITER_KEYS :
			return assoc.first;
		case ITER_VALUES :
			return assoc.second;
		case ITER_ASSOCS :
			{
			SuObject* ob = new SuObject();
			ob->add(assoc.first);
			ob->add(assoc.second);
			return ob;
			}
		default :
			unreachable();
			}
		}
	else if (member == COPY)
		{
		return new SuObjectIter(object, values, include_vec, include_map);
		}
	else if (member == REWIND)
		{
		if (nargs != 0)
			except("usage: objectiter.Rewind()");
		iter.rewind();
		return this;
		}
	else if (member == ITER)
		{
		if (nargs != 0)
			except("usage: objectiter.Iter()");
		return this;
		}
	else
		method_not_found(type(), member);
	}

// tests ============================================================

#include "testing.h"
#include "random.h"

class test_object : public Tests
	{
	TEST (1, number)
		{
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
		for (i = 0; i < N; ++i)
			{
			Value key = random(M);
			if (Value data = ob.get(key))
				ob.put(key, data + 1);
			else
				ob.put(key, 1);
			}
		int total = 0;
		for (i = 0; i < M; ++i)
			{
			Value data = ob.get(i);
			verify(data);
			total += data.integer();
			}
		verify(total == N);
		}

	TEST(2, empty)
		{
		SuObject ob;
		verify(ob.size() == 0);
		verify(! ob.get(0));
		verify(! ob.get(10));
		verify(! ob.get("x"));
		}
	TEST(3, no_match)
		{
		SuObject ob;
		int i;
		for (i = 0; i < 100; ++i)
			ob.put(i, i);
		char s[2] = "a";
		for (i = 0; i < 25; ++i, ++*s)
			ob.put(s, i);

		verify(! ob.get(101));
		verify(! ob.get("z"));
		}
	TEST(4, equal)
		{
		SuObject ob1;
		SuObject ob2;
		verify(ob1 == ob2);
		int i;
		for (i = 0; i < 100; ++i)
			ob1.put(i, i);
		verify(! (ob1 == ob2));
		for (i = 0; i < 100; ++i)
			ob2.put(i, i);
		verify(ob1 == ob2);
		char s[2] = "a";
		for (i = 0; i < 26; ++i, ++*s)
			ob1.put(s, i);

		verify(! (ob1 == ob2));
		*s = 'a';
		for (int j = 0; j < i; ++j, ++*s)
			ob2.put(s, j);
		verify(ob1 == ob2);
		}
	TEST(5, slice)
		{
		int i;

		//building ob1 to offset
		SuObject ob1;
		for (i = 0; i < 100; ++i)
			ob1.put(i, i);
		char s[2] = "a";
		for (i = 0; i < 26; ++i, ++*s)
			ob1.put(s, i);

		SuObject* ob = ob1.slice(2);

		//building ob2 - should be same as ob1 after slice
		SuObject ob2;
		for (i = 0; i < 98; ++i)
			ob2.put(i, i + 2);
		*s = 'a';
		for (i = 0; i < 26; ++i, ++*s)
			ob2.put(s, i);

		verify(*ob == ob2);
		}
	TEST(6, vector)
		{
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
	TEST(7, iterate)
		{
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
		for (i = 0; i < 100; ++i, ++iter)
			{
			verify(iter != ob.end());
			verify((*iter).first == i);
			verify((*iter).second == i);
			}
		for (i = 0; i < 26; ++i, ++iter)
			{
			verify(iter != ob.end());
			std::pair<Value,Value> p = *iter;
			gcstring s = p.first.gcstr();
			verify(s.size() == 1 && s[0] >= 'a' && s[0] <= 'z');
			verify(p.second == s[0] - 'a');
			}
		verify(iter == ob.end());
		}
	TEST(9, remove1)
		{
		SuObject ob;
		ob.remove1(SuTrue);

		ob.add(SuTrue);
		asserteq(ob.size(), 1);
		ob.remove1(SuFalse);
		asserteq(ob.size(), 1);
		ob.remove1(SuTrue);
		asserteq(ob.size(), 0);

		ob.add(1);
		ob.add(2);
		ob.add(4);
		ob.put(10, 2);
		ob.put(11, 3);
		asserteq(ob.size(), 5);
		ob.remove1(2);
		asserteq(ob.size(), 4);
		asserteq(ob.get(0), 1);
		asserteq(ob.get(1), 4);
		asserteq(ob.get(10), 2);
		asserteq(ob.get(11), 3);
		}
	};
REGISTER(test_object);

class test_object2 : public Tests
	{
	TEST (1, list_named)
		{
		asserteq(Value(3), run("#(1, 2, a: 3).Size()"));
		asserteq(Value(2), run("#(1, 2, a: 3).Size(list:)"));
		asserteq(Value(1), run("#(1, 2, a: 3).Size(named:)"));
		asserteq(Value(3), run("#(1, 2, a: 3).Size(list:, named:)"));
		asserteq(Value(2), run("#(1, 2, a: 3).Size(list:, named: false)"));
		asserteq(Value(1), run("#(1, 2, a: 3).Size(list: false, named:)"));
		asserteq(Value(0), run("#(1, 2, a: 3).Size(list: false, named: false)"));

		asserteq(run("#(0, 1, a)"), run("#(1, 2, a: 3).Members()"));
		asserteq(run("#(0, 1)"), run("#(1, 2, a: 3).Members(list:)"));
		asserteq(run("#(a)"), run("#(1, 2, a: 3).Members(named:)"));
		asserteq(run("#(0, 1, a)"), run("#(1, 2, a: 3).Members(list:, named:)"));
		asserteq(run("#(0, 1)"), run("#(1, 2, a: 3).Members(list:, named: false)"));
		asserteq(run("#(a)"), run("#(1, 2, a: 3).Members(list: false, named:)"));
		asserteq(run("#()"), run("#(1, 2, a: 3).Members(list: false, named: false)"));

		asserteq(run("#(1, 2, 3)"), run("#(1, 2, a: 3).Values()"));
		asserteq(run("#(1, 2)"), run("#(1, 2, a: 3).Values(list:)"));
		asserteq(run("#(3)"), run("#(1, 2, a: 3).Values(named:)"));
		asserteq(run("#(1, 2, 3)"), run("#(1, 2, a: 3).Values(list:, named:)"));
		asserteq(run("#(1, 2)"), run("#(1, 2, a: 3).Values(list:, named: false)"));
		asserteq(run("#(3)"), run("#(1, 2, a: 3).Values(list: false, named:)"));
		asserteq(run("#()"), run("#(1, 2, a: 3).Values(list: false, named: false)"));

		asserteq(run("#((0, 1), (1, 2), (a, 3))"), run("#(1, 2, a: 3).Assocs()"));
		asserteq(run("#((0, 1), (1, 2))"), run("#(1, 2, a: 3).Assocs(list:)"));
		asserteq(run("#((a, 3))"), run("#(1, 2, a: 3).Assocs(named:)"));
		asserteq(run("#((0, 1), (1, 2), (a, 3))"), run("#(1, 2, a: 3).Assocs(list:, named:)"));
		asserteq(run("#((0, 1), (1, 2))"), run("#(1, 2, a: 3).Assocs(list:, named: false)"));
		asserteq(run("#((a, 3))"), run("#(1, 2, a: 3).Assocs(list: false, named:)"));
		asserteq(run("#()"), run("#(1, 2, a: 3).Assocs(list: false, named: false)"));
		}
	};
REGISTER(test_object2);
