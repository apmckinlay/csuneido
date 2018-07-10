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
	uint16_t mem;
	};

// database record values
class SuRecord : public SuObject
	{
public:
	SuRecord();
	explicit SuRecord(const SuRecord& rec);
	explicit SuRecord(SuObject* ob);
	SuRecord(const Row& r, const Header& hdr, int t);
	SuRecord(const Row& r, const Header& hdr, SuTransaction* t = 0);
	SuRecord(const Record& rec, const Lisp<int>& flds, SuTransaction* t);

	void out(Ostream& os) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, uint16_t* argnames, int each) override;

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
	void dependencies(uint16_t mem, gcstring s);
	void call_observer(uint16_t member, const char* why);
	void call_observers(uint16_t member, const char* why);

	void add_dependent(uint16_t src, uint16_t dst);
	void invalidate(uint16_t mem);
	void invalidate_dependents(uint16_t member);
	Value get_if_special(uint16_t i);
	Value call_rule(uint16_t i, const char* why);

	Header hdr;
	SuTransaction* trans = nullptr;
	Mmoffset recadr = 0;
	enum { NEW, OLD, DELETED } status = NEW;
	List<Value> observers;
	HashMap<uint16_t,Lisp<uint16_t> > dependents;
	HashMap<uint16_t,bool> invalid;
	ListSet<uint16_t> invalidated;
	Lisp<Value> active_rules;
	List<Observe> active_observers;
	HashMap<uint16_t,Value> attached_rules;
	};

Value su_record();
