#ifndef SUDB_H
#define SUDB_H

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

#include "lisp.h"
#include "gcstring.h"
#include "row.h"
#include "lisp.h"
#include "hashmap.h"
#include "suvalue.h"

class DbmsQuery;
class SuObject;
class SuTransaction;
template <class T1, class T2> class Hashmap;

typedef HashMap<ushort,Value> Rules;

class DatabaseClass : public SuValue
	{
public:
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
	void out(Ostream& os)
		{ os << "Database"; }
	};

// database query values
class SuQuery : public SuValue
	{
	friend class SuRecord;
	friend class SuTransaction;
	friend struct SetTran;
public:
	SuQuery(const gcstring& s, DbmsQuery* n, SuTransaction* trans = 0);
	void out(Ostream& os)
		{ os << "Query(\"" << query << "\")"; }
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
	void close();
protected:
	Value get(Dir);
	Value rewind();
	SuObject* getfields();
	SuObject* getRuleColumns();
	SuObject* getkeys();
	SuObject* getindexes();
	SuObject* getorder();
	Value explain();
	Value output(SuObject* ob);

	gcstring query;
	Header hdr;
	DbmsQuery* q;
	SuTransaction* t;
	enum { NOT_EOF, PREV_EOF, NEXT_EOF } eof;
	bool closed;
	};

Record object_to_record(const Header& hdr, SuObject* ob);

// builtin Transaction value
class TransactionClass : public SuValue
	{
public:
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
	void out(Ostream& os)
		{ os << "Transaction"; }
	};

// database transaction values
class SuTransaction : public SuValue
	{
public:
	enum TranType { READONLY, READWRITE };
	explicit SuTransaction(TranType type);
	explicit SuTransaction(int tran);
	void out(Ostream& os)
		{ os << "Transaction" << tran; }
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
	bool isdone()
		{ return done; }

	Value query(char* s);
	bool commit();
	void rollback();
	void checkNotEnded(char* action);

	char* get_conflict() const
		{ return conflict; }

	int tran;
private:
	bool done;
	char* conflict;
	};

// builtin Cursor value
class CursorClass : public SuValue
	{
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
	void out(Ostream& os)
		{ os << "CursorClass"; }
	};

// database query cursor values
class SuCursor : public SuQuery
	{
public:
	SuCursor(const gcstring& s, DbmsQuery* q) : SuQuery(s, q)
		{ num = next_num++; }
	void out(Ostream& os)
		{ os << "Cursor" << num << "(" << query << ")"; }
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
private:
	int num;
	static int next_num;
	};

#endif
