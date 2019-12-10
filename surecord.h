#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "suobject.h"
#include "row.h"
#include "hashmap.h"
#include "list.h"

class SuTransaction;
class Record;

struct Observe {
	Value fn;
	short mem;
};

// database record values
class SuRecord : public SuObject {
public:
	SuRecord();
	explicit SuRecord(const SuRecord& rec);
	explicit SuRecord(SuObject* ob, int each);
	SuRecord(const Row& r, const Header& hdr, int t);
	SuRecord(const Row& r, const Header& hdr, SuTransaction* t = 0);
	SuRecord(Record rec, const Lisp<gcstring>& flds, SuTransaction* t);

	void out(Ostream& os) const override;
	void show(Ostream& os) const override {
		SuObject::show(os, "[", "]");
	}
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;

	// putdata has to notify observers of changes
	void putdata(Value i, Value x) override;

	bool erase(Value x) override;
	bool erase2(Value x) override;

	Record to_record(const Header& hdr);

	enum { DATABASE = 0xfffe };
	friend class TrackRule;
	friend class TrackObserver;

	void pack(char* buf) const override;
	static SuRecord* unpack(const gcstring& s);

protected:
	SuObject* dup() override {
		return new SuRecord(*this);
	}
	// getdefault adds dependents, handles _lower!, and calls rules
	Value getdefault(Value member, Value def) override;

private:
	void erase();
	void update();
	void update(SuObject* rec);
	void ck_modify(const char* op);

	void init(const Row& r);
	void addfield(const char* field, gcstring value);
	void dependencies(short mem, gcstring s);
	void call_observer(short member, const char* why);
	void call_observers(short member, const char* why);

	void add_dependent(short src, short dst);
	void invalidate(short mem);
	void invalidate_dependents(short member);
	Value get_if_special(short i);
	Value call_rule(short i, const char* why);

	Header hdr;
	SuTransaction* trans = nullptr;
	Mmoffset recadr = 0;
	enum { NEW, OLD, DELETED } status = NEW;
	List<Value> observers;
	HashMap<short, List<short>> dependents;
	HashMap<short, bool> invalid;
	ListSet<short> invalidated;
	List<Value> active_rules;
	List<Observe> active_observers;
	HashMap<short, Value> attached_rules;
};

Value su_record();
