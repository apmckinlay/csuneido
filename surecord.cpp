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

#include "surecord.h"
#include "sudb.h"
#include "sustring.h"
#include "interp.h"
#include "globals.h"
#include "symbols.h"
#include "func.h" // for argseach
#include "except.h"
#include "ostreamstr.h"
#include "suboolean.h"
#include "dbms.h"
#include "record.h"
#include "pack.h"
#include "catstr.h"
#include "commalist.h"
#include "sublock.h"

//#define LOG
#ifdef LOG
#include "ostreamfile.h"
OstreamFile osf("surecord.log");
#define log(args) ((osf << (void*) this << ": " << args << endl), osf.flush())
#else
#define log(args)
#endif

SuRecord::SuRecord() 
	: trans(0), recadr(0), status(NEW)
	{
	log("create");
	}

SuRecord::SuRecord(const SuRecord& rec)
	: SuObject(rec), trans(0), recadr(0), status(rec.status),
	observer(rec.observer.copy()), dependents(rec.dependents),
	invalid(rec.invalid), invalidated(rec.invalidated.copy())
	// note: only have to copy() lists that are appended to
	{
	log("create");
	}

static ushort basename(char* field)
	{
	return ::symnum(PREFIXA(field, strlen(field) - 5));
	}

SuRecord::SuRecord(const Row& r, const Header& h, int t)
	: hdr(h), trans(t <= 0 ? 0 : new SuTransaction(t)), recadr(r.recadr), status(OLD)
	{
	init(r);
	}

SuRecord::SuRecord(const Row& r, const Header& h, SuTransaction* t)
	: hdr(h), trans(t), recadr(r.recadr), status(OLD)
	{
	init(r);
	}

void SuRecord::init(const Row& dbrow)
	{
	verify(recadr >= 0);
	log("create");
	Row row(dbrow);
	row.to_heap();
	// TODO: cache symbol's
	for (Row::iterator iter = row.begin(hdr); iter != row.end(); ++iter)
		{
		std::pair<gcstring,gcstring> p = *iter;
		if (p.first == "-" || p.second.size() == 0)
			continue ;
		char* field = p.first.str();
		Value x = ::unpack(p.second);
		if (has_suffix(field, "_deps"))
			dependencies(basename(field), x.gcstr());
		else
			put(field, x);
		}
	}

SuRecord::SuRecord(const Record& dbrec, const Lisp<int>& fldsyms, SuTransaction* t)
	: trans(t), recadr(0), status(OLD)
	{
	log("create");
	Record rec = dbrec.to_heap();
	int i = 0;
	for (Lisp<int> f = fldsyms; ! nil(f); ++f, ++i)
		{
		if (*f == -1)
			continue ;
		Value x = ::unpack(rec.getraw(i));
		// dependencies
		char* field = symstr(*f);
		if (has_suffix(field, "_deps"))
			{
			char* base = PREFIXA(field, strlen(field) - 5);
			dependencies(::symnum(base), x.gcstr());
			}
		else
			put(symbol(*f), x);
		}
	}

void SuRecord::dependencies(ushort mem, gcstring s)
	{
	for (;;)
		{
		int i = s.find(',');
		gcstring t = s.substr(0, i);
		ushort m = ::symnum(t.str());
		dependents[m].push(mem);
		if (i == -1)
			break ;
		s = s.substr(i + 1);
		}
	}

static ushort depsname(ushort m)
	{
	return ::symnum(CATSTRA(symstr(m), "_deps"));
	}

// NOTE: hdr argument overrides hdr member
Record SuRecord::to_record(const Header& hdr)
	{
	const Lisp<int> fldsyms = hdr.output_fldsyms();
	// dependencies
	// - access all the fields to ensure dependencies are created
	Lisp<int> f;
	for (f = fldsyms; ! nil(f); ++f)
		if (*f != -1)
			getdata(symbol(*f));
	// - invert stored dependencies
	typedef HashMap<ushort, Lisp<ushort> > Deps;
	Deps deps;
	for (HashMap<ushort,Lisp<ushort> >::iterator it = dependents.begin(); 
		it != dependents.end(); ++it)
		{
		for (Lisp<ushort> m = it->val; ! nil(m); ++m)
			{
			ushort d = depsname(*m);
			if (fldsyms.member(d))
				deps[d].push(it->key);
			}
		}

	Record rec;
	OstreamStr oss;
	int ts = hdr.timestamp_field();
	for (f = fldsyms; ! nil(f); ++f)
		{
		if (*f == -1)
			rec.addnil();
		else if (*f == ts)
			rec.addval(dbms()->timestamp());
		else if (Lisp<ushort>* pd = deps.find(*f))
			{
			// output dependencies
			oss.clear();
			for (Lisp<ushort> d = *pd; ! nil(d); )
				{
				oss << symstr(*d);
				if (! nil(++d))
					oss << ",";
				}
			rec.addval(oss.str());
			}
		else if (Value x = getdata(symbol(*f)))
			rec.addval(x);
		else
			rec.addnil();
		}
	return rec;
	}

void SuRecord::out(Ostream& os)
	{
	if (this == globals["Record"].ptr())
		os << "Record /* builtin */";
	else
		SuObject::outdelims(os, "[]");
	}

Value SuRecord::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value Clear("Clear");
	static Value Transaction("Transaction");
	static Value Query("Query");
	static Value Newq("New?");
	static Value Copy("Copy");
	static Value Invalidate("Invalidate");
	static Value Update("Update");
	static Value Delete("Delete");
	static Value Observer("Observer");
	static Value PreSet("PreSet");
	static Value Set_default("Set_default");
	static Value GetDeps("GetDeps");
	static Value SetDeps("SetDeps");
	static Value Add("Add");

	if (member != Add) // Add optimizes each
		argseach(nargs, nargnames, argnames, each);
	if (member == Clear)
		{
		SuRecord empty;
		*this = empty;
		return Value();
		}
	else if (member == Transaction)
		return trans && ! trans->isdone() ? (Value) trans : SuFalse;
	else if (member == Newq)
		return status == NEW ? SuTrue : SuFalse;
	else if (member == Copy)
		{
		if (nargs != 0)
			except("usage: record.Copy()");
		SuRecord* r = new SuRecord(*this);
		r->observer = Lisp<Value>();
		return r;
		}
	else if (member == Invalidate)
		{
		if (nargnames != 0)
			except("usage: record.Invalidate(member ...)");
		for (int i = 0; i < nargs; ++i)
			{
			ushort m = ::symnum(ARG(i).str());
			invalidate(m);
			call_observers(m);
			}
		return Value();
		}
	else if (member == INSTANTIATE || member == CALL)
		{
		static Value gRecord = globals["Record"];
		if (self.ptr() != gRecord.ptr())
			except("method not found: Call");
		SuRecord* ob = new SuRecord;
		// convert args to members
		Value* args = GETSP() - nargs + 1;
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
	else if (member == Update)
		{
		if (nargs == 0)
			update();
		else if (nargs == 1)
			{
			if (SuTransaction* st = dynamic_cast<SuTransaction*>(ARG(0).ptr()))
				{
				if (! recadr)
					except("can only Update records read from the database");
				if (! dbms()->record_ok(st->tran, recadr))
					except("can't update - record had been modified");
				trans = st;
				update();
				}
			else
				update(ARG(0).object());
			}
		else if (nargs != 0)
			except("usage: record.Update() or record.Update(object) or record.Update(transaction)");
		return SuTrue;
		}
	else if (member == Delete)
		{
		SuTransaction* st;
		if (nargs == 1 && (st = dynamic_cast<SuTransaction*>(ARG(0).ptr())))
			{
			if (! recadr)
				except("can only Delete records read from the database");
			if (! dbms()->record_ok(st->tran, recadr))
				except("can't delete - record had been modified");
			trans = st;
			}
		else if (nargs != 0)
			return SuObject::call(self, member, nargs, nargnames, argnames, each);
		erase();
		return SuTrue;
		}
	else if (member == Observer)
		{
		if (nargs != 1)
			except("usage: record.Observer(observer)");
		persist_if_block(ARG(0));
		observer.append(ARG(0));
		return Value();
		}
	else if (member == PreSet)
		{ // e.g. record.PreSet(field, value)
		if (nargs != 2)
			except("usage: record.PreSet(field, value)");
		SuObject::putdata(ARG(0), ARG(1));
		return Value();
		}
	else if (member == Set_default)
		{
		if (nargs != 1 || ARG(0) != SuEmptyString)
			except("Record does not support Set_default");
		return this;
		}
	else if (member == GetDeps)
		{
		if (nargs != 1)
			except("usage: record.GetDeps(field)");
		int mem = ARG(0).symnum();
		gcstring deps;
		for (HashMap<ushort,Lisp<ushort> >::iterator it = dependents.begin(); 
			it != dependents.end(); ++it)
			{
			for (Lisp<ushort> m = it->val; ! nil(m); ++m)
				if (*m == mem)
					{
					deps += ",";
					deps += symstr(it->key);
					}
			}
		return new SuString(deps.substr(1));
		}
	else if (member == SetDeps)
		{
		if (nargs != 2)
			except("usage: record.SetDeps(field, comma_string)");
		dependencies(ARG(0).symnum(), ARG(1).gcstr());
		return Value();
		}
	else
		{
		// TODO: calling object methods shouldn't require Records
		static ushort gRecords = globals("Records");
		Value Records = globals.find(gRecords);
		SuObject* ob;
		if (Records && (ob = Records.ob_if_ob()) && ob->has(member))
			return ob->call(self, member, nargs, nargnames, argnames, each);
		else
			return SuObject::call(self, member, nargs, nargnames, argnames, each);
		}
	}

void SuRecord::erase()
	{
	ck_modify("Delete");
	status = DELETED;
	dbms()->erase(trans->tran, recadr);
	}

void SuRecord::update()
	{
	ck_modify("Update");
	Record newrec =  to_record(hdr);
	recadr = dbms()->update(trans->tran, recadr, newrec);
	verify(recadr >= 0);
	}

void SuRecord::update(SuObject* ob)
	{
	ck_modify("Update");
	Record newrec = object_to_record(hdr, ob);
	recadr = dbms()->update(trans->tran, recadr, newrec);
	verify(recadr >= 0);
	}

void SuRecord::ck_modify(char* op)
	{
	if (! trans)
		except("record." << op << ": no Transaction");
	if (trans->isdone())
		except("record." << op << ": Transaction already completed");
	if (status != OLD || ! recadr)
		except("record." << op << ": not a database record");
	}

void SuRecord::putdata(Value m, Value x)
	{
	log("putdata " << m << " = " << x);
	Value old = SuObject::getdata(m);
	if (old && x == old)
		return ;
	SuObject::putdata(m, x);
	int i = m.symnum();
	invalidate_dependents(i);
	call_observers(i);
	}

void SuRecord::call_observers(ushort i)
	{
	call_observer(i);
	for (; ! nil(invalidated); ++invalidated)
		if (*invalidated != i)
			call_observer(*invalidated);
	}

void SuRecord::invalidate_dependents(ushort mem)
	{
	log("invalidate dependents of " << symstr(mem));
	for (Lisp<ushort> m = dependents[mem]; ! nil(m); ++m)
		invalidate(*m);
	}

void SuRecord::invalidate(ushort mem)
	{
	log("invalidate " << symstr(mem));
	bool was_valid = ! invalid.find(mem);
	if (! member(invalidated, mem))
		invalidated.append(mem); // for observers
	invalid[mem] = true;
	if (was_valid)
		invalidate_dependents(mem);
	}

// track active observers to prevent cycles
class TrackObserver
	{
public:
	explicit TrackObserver(SuRecord* r, Value observer, ushort member) : rec(r)
		{ r->active_observers.push(Observe(observer, member)); }
	static bool has(SuRecord* r, Value observer, ushort member)
		{
		for (Lisp<Observe> obs = r->active_observers; ! nil(obs); ++obs)
			if (obs->fn == observer && obs->mem == member)
				return true;
		return false;
		}
	~TrackObserver()
		{ rec->active_observers.pop(); }
private:
	SuRecord* rec;
	};

void SuRecord::call_observer(ushort member)
	{
	log("call_observer " << symstr(member));
	static ushort argname = ::symnum("member");
	for (Lisp<Value> obs = observer; ! nil(obs); ++obs)
		{
		// prevent cycles
		if (TrackObserver::has(this, *obs, member))
			continue ;
		TrackObserver track(this, *obs, member);

		KEEPSP
		PUSH(symbol(member));
		(*obs).call(this, CALL, 1, 1, &argname, -1);
		}
	}

Value SuRecord::getdata(Value m)
	{
	log("getdata " << m);
	int i = m.symnum();
	if (tss_proc()->fp->rule.rec == this)
		add_dependent(tss_proc()->fp->rule.mem, i);
	Value result = get(m);
	if (! result || invalid.find(i))
		{
		if (Value x = call_rule(i))
			result = x;
		else if (! result)
			result = SuString::empty_string;
		}
	return result;
	}

void SuRecord::add_dependent(ushort src, ushort dst)
	{
	log("add_dependent " << symstr(src) << " on " << symstr(dst));
	Lisp<ushort>& list = dependents[dst];
	if (! member(list, src))
		list.push(src);
	}

// prevent rules from triggering themselves
class TrackRule
	{
public:
	explicit TrackRule(SuRecord* r, Value rule) : rec(r)
		{ r->active_rules.push(rule); }
	static bool has(SuRecord* r, Value rule)
		{ return r->active_rules.member(rule); }
	~TrackRule()
		{ rec->active_rules.pop(); }
private:
	SuRecord* rec;
	};

Value SuRecord::call_rule(ushort i)
	{
	invalid.erase(i);

	if (! (i & 0x8000))
		return Value();
	Value fn = globals.find(CATSTRA("Rule_", symstr(i)));

	if (! fn || TrackRule::has(this, fn))
		return Value();
	TrackRule tr(this, fn);

	log("call_rule " << symstr(i));

	Rule old_rule = tss_proc()->fp->rule;
	tss_proc()->fp->rule.rec = this;
	tss_proc()->fp->rule.mem = i;

	KEEPSP
	Value x = fn.call(this, CALL, 0, 0, 0, -1);

	tss_proc()->fp->rule = old_rule; // BUG not restored if exception?
	
	if (x)
		put(symbol(i), x);
	return x;
	}

void SuRecord::pack(char* buf) const
	{
	SuObject::pack(buf);
	*buf = PACK_RECORD;
	}

/*static*/ SuRecord* SuRecord::unpack(const gcstring& s)
	{
	SuRecord* r = new SuRecord;
	unpack2(s, r);
	return r;
	}

