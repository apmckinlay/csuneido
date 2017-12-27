#pragma once
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
#include "lisp.h"
#include "row.h"
#include "hashmap.h"
#include "list.h"

class SuTransaction;
class Record;

struct Observe
	{
	Value fn;
	ushort mem;
	};

// database record values
class SuRecord : public SuObject
	{
public:
	SuRecord();
	explicit SuRecord(const SuRecord& rec);
	SuRecord(const Row& r, const Header& hdr, int t);
	SuRecord(const Row& r, const Header& hdr, SuTransaction* t = 0);
	SuRecord(const Record& rec, const Lisp<int>& flds, SuTransaction* t);

	void out(Ostream& os) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;

	// putdata has to notify observers of changes
	void putdata(Value i, Value x) override;
	// getdata has to auto-register a rule as an observer
	Value getdata(Value) override;

	bool erase(Value x) override;
	bool erase2(Value x) override;


	Record to_record(const Header& hdr);

	enum { DATABASE = 0xfffe };
	friend class TrackRule;
	friend class TrackObserver;

	void pack(char* buf) const override;
	static SuRecord* unpack(const gcstring& s);

private:
	void erase();
	void update();
	void update(SuObject* rec);
	void ck_modify(const char* op);

	void init(const Row& r);
	void addfield(const char* field, gcstring value);
	void dependencies(ushort mem, gcstring s);
	void call_observer(ushort member, const char* why);
	void call_observers(ushort member, const char* why);

	void add_dependent(ushort src, ushort dst);
	void invalidate(ushort mem);
	void invalidate_dependents(ushort member);
	Value get_if_special(ushort i);
	Value call_rule(ushort i, const char* why);

	Header hdr;
	SuTransaction* trans;
	Mmoffset recadr;
	enum { NEW, OLD, DELETED } status;
	List<Value> observers;
	HashMap<ushort,Lisp<ushort> > dependents;
	HashMap<ushort,bool> invalid;
	ListSet<ushort> invalidated;
	Lisp<Value> active_rules;
	List<Observe> active_observers;
	HashMap<ushort,Value> attached_rules;
	};

class SuRecordClass : public SuValue
	{
public:
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;
	void out(Ostream& os) const override
		{ os << "Record /* builtin */"; }
	};
