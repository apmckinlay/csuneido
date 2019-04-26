// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "surecord.h"
#include "sudb.h"
#include "sustring.h"
#include "interp.h"
#include "globals.h"
#include "symbols.h"
#include "func.h" // for argseach
#include "exceptimp.h"
#include "ostreamstr.h"
#include "dbms.h"
#include "record.h"
#include "pack.h"
#include "catstr.h"
#include "sublock.h"
#include "trace.h"

#define RTRACE(stuff) TRACE(RECORDS, (void*) this << " " << stuff) // NOLINT

SuRecord::SuRecord() {
	defval = SuEmptyString;
}

SuRecord::SuRecord(const SuRecord& rec)
	: SuObject(rec), status(rec.status), dependents(rec.dependents),
	  invalid(rec.invalid) {
	defval = SuEmptyString;
}

SuRecord::SuRecord(SuObject* ob, int each) : SuObject(ob, each) {
	defval = SuEmptyString;
}

SuRecord::SuRecord(const Row& r, const Header& h, int t)
	: hdr(h), trans(t <= 0 ? nullptr : new SuTransaction(t)), recadr(r.recadr),
	  status(OLD) {
	init(r);
}

SuRecord::SuRecord(const Row& r, const Header& h, SuTransaction* t)
	: hdr(h), trans(t), recadr(r.recadr), status(OLD) {
	init(r);
}

bool isSpecialField(const gcstring& col);

void SuRecord::init(const Row& dbrow) {
	verify(recadr >= 0);
	defval = SuEmptyString;
	Row row(dbrow);
	for (Row::iterator iter = row.begin(hdr); iter != row.end(); ++iter) {
		std::pair<gcstring, gcstring> p = *iter;
		if (p.first != "-" && !isSpecialField(p.first))
			addfield(p.first.str(), p.second);
	}
}

SuRecord::SuRecord(Record rec, const Lisp<gcstring>& flds, SuTransaction* t)
	: trans(t), recadr(0), status(OLD) {
	defval = SuEmptyString;
	int i = 0;
	for (auto f = flds; !nil(f); ++f, ++i)
		if (*f != "-")
			addfield(f->str(), rec.getraw(i));
}

static short basename(const char* field) {
	return ::symnum(PREFIXA(field, strlen(field) - 5));
}

bool getSystemOption(const char* option, bool def_value);

static bool skipField(const char* field, gcstring value) {
	if (value != "")
		return false;
	if (getSystemOption("CreateRuleFieldsInDbRecords", false) &&
		globals.find(CATSTRA("Rule_", field)))
		return false;
	return true;
}

void SuRecord::addfield(const char* field, gcstring value) {
	if (skipField(field, value))
		return;
	Value x = ::unpack(value);
	if (has_suffix(field, "_deps"))
		dependencies(basename(field), x.gcstr());
	else
		put(field, x);
}

void SuRecord::dependencies(short mem, gcstring s) {
	for (;;) {
		int i = s.find(',');
		gcstring t = s.substr(0, i).trim();
		short m = ::symnum(t.str());
		auto& ds = dependents[m];
		if (!ds.has(mem))
			ds.push(mem);
		if (i == -1)
			break;
		s = s.substr(i + 1);
	}
}

static short depsname(short m) {
	return ::symnum(CATSTRA(symstr(m), "_deps"));
}

Record SuRecord::to_record(const Header& h) {
	const Lisp<int> fldsyms = h.output_fldsyms();
	// dependencies
	// - access all the fields to ensure dependencies are created
	Lisp<int> f;
	for (f = fldsyms; !nil(f); ++f)
		if (*f != -1)
			getdata(symbol(*f));
	// - invert stored dependencies
	typedef HashMap<short, List<short>> Deps;
	Deps deps;
	for (auto& entry : dependents) {
		for (short d : entry.val) {
			short dn = depsname(d);
			if (fldsyms.member(dn))
				deps[dn].push(entry.key);
		}
	}

	Record rec;
	OstreamStr oss;
	int ts = h.timestamp_field();
	Value tsval;
	for (f = fldsyms; !nil(f); ++f) {
		if (*f == -1)
			rec.addnil();
		else if (*f == ts)
			rec.addval(tsval = dbms()->timestamp());
		else if (List<short>* pd = deps.find(*f)) {
			// output dependencies
			oss.clear();
			auto sep = "";
			for (short d : *pd) {
				oss << sep << symstr(d);
				sep = ",";
			}
			rec.addval(oss.str());
		} else if (Value x = getdata(symbol(*f)))
			rec.addval(x);
		else
			rec.addnil();
	}
	if (tsval && !get_readonly())
		put(symbol(ts), tsval);
	return rec;
}

void SuRecord::out(Ostream& os) const {
	if (this == globals["Record"].ptr())
		os << "Record /* builtin */";
	else
		SuObject::outdelims(os, "[]");
}

Value SuRecord::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	static Value Clear("Clear");
	static Value Transaction("Transaction");
	static Value Newq("New?");
	static Value Copy("Copy");
	static Value Invalidate("Invalidate");
	static Value Update("Update");
	static Value Delete("Delete");
	static Value Observer("Observer");
	static Value RemoveObserver("RemoveObserver");
	static Value PreSet("PreSet");
	static Value GetDeps("GetDeps");
	static Value SetDeps("SetDeps");
	static Value Add("Add");
	static Value AttachRule("AttachRule");

	if (member != Add) // Add optimizes each
		argseach(nargs, nargnames, argnames, each);
	if (member == Clear) {
		SuRecord empty;
		*this = empty;
		return Value();
	} else if (member == Transaction)
		return trans && !trans->isdone() ? (Value) trans : SuFalse;
	else if (member == Newq)
		return status == NEW ? SuTrue : SuFalse;
	else if (member == Copy) {
		NOARGS("record.Copy()");
		return new SuRecord(*this);
	} else if (member == Invalidate) {
		if (nargnames != 0)
			except("usage: record.Invalidate(member ...)");
		for (int i = 0; i < nargs; ++i) {
			RTRACE(".Invalidate " << ARG(i).str());
			short m = ::symnum(ARG(i).str());
			invalidate(m);
			call_observers(m, ".Invalidate");
		}
		return Value();
	} else if (member == Update) {
		if (nargs == 0)
			update();
		else if (nargs == 1)
			update(ARG(0).object());
		else
			except("usage: record.Update() or record.Update(object))");
		return Value();
	} else if (member == Delete) {
		if (nargs == 0) {
			erase();
			return Value();
		} else
			return SuObject::call(
				self, member, nargs, nargnames, argnames, each);
	} else if (member == Observer) {
		if (nargs != 1)
			except("usage: record.Observer(observer)");
		persist_if_block(ARG(0));
		observers.add(ARG(0));
		return Value();
	} else if (member == RemoveObserver) {
		if (nargs != 1)
			except("usage: record.RemoveObserver(observer)");
		observers.erase(ARG(0));
		return Value();
	} else if (member == PreSet) { // e.g. record.PreSet(field, value)
		if (nargs != 2)
			except("usage: record.PreSet(field, value)");
		SuObject::putdata(ARG(0), ARG(1));
		return Value();
	} else if (member == GetDeps) {
		if (nargs != 1)
			except("usage: record.GetDeps(field)");
		int mem = ARG(0).symnum();
		gcstring deps;
		for (auto& entry : dependents)
			for (short d : entry.val)
				if (d == mem) {
					deps += ",";
					deps += symstr(entry.key);
				}
		return new SuString(deps.substr(1));
	} else if (member == SetDeps) {
		if (nargs != 2)
			except("usage: record.SetDeps(field, comma_string)");
		dependencies(ARG(0).symnum(), ARG(1).gcstr());
		return Value();
	} else if (member == AttachRule) {
		if (nargs != 2)
			except("usage: record.AttachRule(field, callable)");
		attached_rules[ARG(0).symnum()] = ARG(1);
		return Value();
	} else {
		// TODO: calling object methods shouldn't require Records
		static UserDefinedMethods udm("Records");
		if (Value c = udm(member))
			return c.call(self, member, nargs, nargnames, argnames, each);
		return SuObject::call(self, member, nargs, nargnames, argnames, each);
	}
}

void SuRecord::erase() {
	ck_modify("Delete");
	status = DELETED;
	dbms()->erase(trans->tran, recadr);
}

void SuRecord::update() {
	ck_modify("Update");
	Record newrec = to_record(hdr);
	recadr = dbms()->update(trans->tran, recadr, newrec);
	verify(recadr >= 0);
}

void SuRecord::update(SuObject* ob) {
	ck_modify("Update");
	Record newrec = object_to_record(hdr, ob);
	recadr = dbms()->update(trans->tran, recadr, newrec);
	verify(recadr >= 0);
}

void SuRecord::ck_modify(const char* op) {
	if (!trans)
		except("record." << op << ": no Transaction");
	if (trans->isdone())
		except("record." << op << ": Transaction already completed");
	if (status != OLD || !recadr)
		except("record." << op << ": not a database record");
}

void SuRecord::putdata(Value m, Value x) {
	RTRACE("set " << m << " = " << x);
	int i = m.symnum();
	invalid.erase(i); // before getdata
	Value old = get(m);
	SuObject::putdata(m, x);
	if (old && x == old)
		return;
	invalidate_dependents(i);
	call_observers(i, "set");
}

bool SuRecord::erase(Value m) {
	bool result = SuObject::erase(m);
	if (result) {
		int i = m.symnum();
		invalidate_dependents(i);
		call_observers(i, "delete");
	}
	return result;
}

bool SuRecord::erase2(Value m) {
	bool result = SuObject::erase2(m);
	if (result) {
		int i = m.symnum();
		invalidate_dependents(i);
		call_observers(i, "erase");
	}
	return result;
}

void SuRecord::call_observers(short i, const char* why) {
	call_observer(i, why);
	while (!invalidated.empty())
		if (auto x = invalidated.popfront(); x != i)
			call_observer(x, "invalidate");
}

void SuRecord::invalidate_dependents(short mem) {
	RTRACE("invalidate dependents of " << symstr(mem));
	for (short d : dependents[mem])
		invalidate(d);
}

void SuRecord::invalidate(short mem) {
	// TODO maybe clear dependencies?
	// (would give a way to safely clear dependencies)
	RTRACE("invalidate " << symstr(mem));
	bool was_valid = !invalid.find(mem);
	invalidated.add(mem); // for observers
	invalid[mem] = true;
	if (was_valid)
		invalidate_dependents(mem);
}

bool operator==(Observe o1, Observe o2) {
	return o1.fn == o2.fn && o1.mem == o2.mem;
}

// track active observers to prevent cycles
class TrackObserver {
public:
	explicit TrackObserver(SuRecord* r, Value observer, short mem) : rec(r) {
		r->active_observers.push(Observe{observer, mem});
	}
	static bool has(SuRecord* r, Value observer, short mem) {
		return r->active_observers.has(Observe{observer, mem});
	}
	~TrackObserver() {
		rec->active_observers.pop();
	}

private:
	SuRecord* rec;
};

void SuRecord::call_observer(short member, const char* why) {
	RTRACE("call observer for " << symstr(member) << " due to " << why);
	static short argname = ::symnum("member");
	for (auto o : observers) {
		// prevent cycles
		if (TrackObserver::has(this, o, member))
			continue;
		TrackObserver track(this, o, member);

		KEEPSP
		PUSH(symbol(member));
		o.call(this, CALL, 1, 1, &argname, -1);
	}
}

Value SuRecord::getdata(Value m) {
	int i = m.symnum();
	if (tls().proc->fp->rule.rec == this)
		add_dependent(tls().proc->fp->rule.mem, i);
	Value result = get(m);
	if (!result || invalid.find(i)) {
		if (Value x = get_if_special(i))
			result = x;
		else if (Value y = call_rule(i, result ? "invalid" : "missing"))
			result = y;
		else if (!result)
			result = defval;
	}
	return result;
}

Value SuRecord::get_if_special(short i) {
	gcstring name = symstr(i);
	if (!name.has_suffix("_lower!"))
		return Value();
	gcstring base = name.substr(0, name.size() - 7);
	Value x = get(symbol(base));
	if (auto s = val_cast<SuString*>(x))
		return s->tolower();
	return x;
}

void SuRecord::add_dependent(short src, short dst) {
	if (src == dst)
		return;
	auto& list = dependents[dst];
	if (!list.has(src)) {
		RTRACE("add dependency for " << symstr(src) << " uses " << symstr(dst));
		list.add(src);
	}
}

// prevent rules from triggering themselves
class TrackRule {
public:
	explicit TrackRule(SuRecord* r, Value rule) : rec(r) {
		r->active_rules.push(rule);
	}
	static bool has(SuRecord* r, Value rule) {
		return r->active_rules.has(rule);
	}
	~TrackRule() {
		rec->active_rules.pop();
	}

private:
	SuRecord* rec;
};

Value SuRecord::call_rule(short i, const char* why) {
	invalid.erase(i);

	if (!(i & 0x8000))
		return Value();
	Value fn;
	if (Value* pv = attached_rules.find(i))
		fn = *pv;
	else if (defval)
		fn = globals.find(CATSTRA("Rule_", symstr(i)));

	if (!fn || TrackRule::has(this, fn))
		return Value();
	TrackRule tr(this, fn);

	RTRACE("call rule " << symstr(i) << " - " << why);

	Rule old_rule = tls().proc->fp->rule;
	tls().proc->fp->rule.rec = this;
	tls().proc->fp->rule.mem = i;

	KEEPSP
	Value x;
	try {
		x = fn.call(this, CALL);
	} catch (const Except& e) {
		// TODO handle block return ?
		tls().proc->fp->rule = old_rule;
		throw Except(e, e.gcstr() + " (Rule_" + symstr(i) + ")");
	}

	tls().proc->fp->rule = old_rule;

	if (x && !get_readonly())
		put(symbol(i), x);
	return x;
}

void SuRecord::pack(char* buf) const {
	SuObject::pack(buf);
	*buf = PACK_RECORD;
}

/*static*/ SuRecord* SuRecord::unpack(const gcstring& s) {
	SuRecord* r = new SuRecord;
	unpack2(s, r);
	return r;
}

// Record(...) function

class MkRecord : public Func {
public:
	MkRecord() {
		named.num = globals("Record");
	}
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
};

Value su_record() {
	return new MkRecord();
}

Value MkRecord::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	if (member != CALL)
		return Func::call(self, member, nargs, nargnames, argnames, each);
	Value* args = GETSP() - nargs + 1;
	if (each >= 0) {
		verify(nargs == 1 && nargnames == 0);
		return new SuRecord(args[0].object(), each);
	}
	SuRecord* ob = new SuRecord;
	// convert args to members
	short unamed = nargs - nargnames;
	// un-named
	int i;
	for (i = 0; i < unamed; ++i)
		ob->put(i, args[i]);
	// named
	verify(i >= nargs || argnames);
	for (int j = 0; i < nargs; ++i, ++j)
		ob->put(symbol(argnames[j]), args[i]);
	return ob;
}
