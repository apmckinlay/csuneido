#ifndef SURECORD_H
#define SURECORD_H

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

class SuTransaction;
class Record;

struct Observe
	{
	Observe(Value f, ushort m) : fn(f), mem(m)
		{ }
	Value fn; 
	ushort mem;
	};

// database record values
class SuRecord : public SuObject
	{
public:
	explicit SuRecord();
	explicit SuRecord(const SuRecord& rec);
	SuRecord(const Row& r, const Header& hdr, int t);
	SuRecord(const Row& r, const Header& hdr, SuTransaction* t = 0);
	SuRecord(const Record& rec, const Lisp<int>& flds, SuTransaction* t);

	virtual void out(Ostream& os);
	virtual Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);

	// putdata has to notify observers of changes
	virtual void putdata(Value i, Value x);
	// getdata has to auto-register a rule as an observer
	virtual Value getdata(Value);

	Value call_rule(ushort i);

	Record to_record(const Header& hdr);

	enum { DATABASE = 0xfffe };
	friend class TrackRule;
	friend class TrackObserver;

	void pack(char* buf) const;
	static SuRecord* unpack(const gcstring& s);

private:
	void erase();
	void update();
	void update(SuObject* rec);
	void ck_modify(char* op);

	void init(const Row& r);
	void addfield(char* field, gcstring value);
	void dependencies(ushort mem, gcstring s);
	void call_observer(ushort member);
	void call_observers(ushort member);

	void add_dependent(ushort src, ushort dst);
	void invalidate(ushort mem);
	void invalidate_dependents(ushort member);

	Header hdr;
	SuTransaction* trans;
	Mmoffset recadr;
	enum { NEW, OLD, DELETED } status;
	Lisp<Value> observer;
	HashMap<ushort,Lisp<ushort> > dependents;
	HashMap<ushort,bool> invalid;
	Lisp<ushort> invalidated;
	Lisp<Value> active_rules;
	Lisp<Observe> active_observers;
	};

class SuRecordClass : public SuValue
	{
public:
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
	void out(Ostream& os)
		{ os << "Record /* builtin */"; }
	};

#endif
