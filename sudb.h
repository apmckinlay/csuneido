#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "gcstring.h"
#include "row.h"
#include "hashmap.h"
#include "value.h"
#include "dir.h"

class DbmsQuery;
class SuObject;
class SuTransaction;
template <class T1, class T2>
class Hashmap;

typedef HashMap<uint16_t, Value> Rules;

// database query values
class SuQuery : public SuValue {
	friend class SuRecord;
	friend class SuTransaction;
	friend struct SetTran;

public:
	SuQuery(const gcstring& s, DbmsQuery* n, SuTransaction* trans = 0);
	void out(Ostream& os) const override {
		os << "Query(\"" << query << "\")";
	}
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
	void close();

protected:
	Value get(Dir);
	Value rewind();
	SuObject* getfields() const;
	SuObject* getRuleColumns() const;
	SuObject* getkeys() const;
	SuObject* getorder() const;
	Value explain() const;
	Value output(SuObject* ob) const;

	gcstring query;
	Header hdr;
	DbmsQuery* q;
	SuTransaction* t;
	enum { NOT_EOF, PREV_EOF, NEXT_EOF } eof;
	bool closed;
};

Record object_to_record(const Header& hdr, SuObject* ob);

// builtin Transaction value
class TransactionClass : public SuValue {
public:
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
	void out(Ostream& os) const override {
		os << "Transaction";
	}
};

// database transaction values
class SuTransaction : public SuValue {
public:
	enum TranType { READONLY, READWRITE };
	explicit SuTransaction(TranType type);
	explicit SuTransaction(int tran);
	void out(Ostream& os) const override {
		os << "Transaction" << tran;
	}
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
	bool isdone() const {
		return done;
	}

	Value query(const char* s);
	bool commit();
	void rollback();
	void checkNotEnded(const char* action);

	const char* get_conflict() const {
		return conflict;
	}

	int tran;

private:
	bool done = false;
	const char* conflict = "";
	Value data;
};

// builtin Cursor value
class CursorClass : public SuValue {
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
	void out(Ostream& os) const override {
		os << "CursorClass";
	}
};

// database query cursor values
class SuCursor : public SuQuery {
public:
	SuCursor(const gcstring& s, DbmsQuery* q) : SuQuery(s, q) {
		num = next_num++;
	}
	void out(Ostream& os) const override {
		os << "Cursor" << num << "(" << query << ")";
	}
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;

private:
	int num;
	static int next_num;
};
