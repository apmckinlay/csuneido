#ifndef DBMS_H
#define DBMS_H

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

#include "dir.h"
#include "mmoffset.h"

// Dbms - pure virtual interface class for Database and Query packages

class Record;
class Row;
class Header;
template <class T> class Lisp;
class gcstring;
class SuValue;

class DbmsQuery
	{
public:
	virtual ~DbmsQuery()
		{ }
	virtual Header header() = 0;
	virtual Lisp<gcstring> order() = 0;
	virtual Lisp<Lisp<gcstring> > keys() = 0;
	virtual Row get(Dir dir) = 0;
	virtual void rewind() = 0;
	virtual char* explain() = 0;
	virtual bool output(const Record& rec) = 0;
	virtual void set_transaction(int tran) = 0;
	virtual void close() = 0;
	};

class Value;

class Dbms
	{
public:
	virtual ~Dbms();

	enum TranType { READONLY, READWRITE };
	virtual int transaction(TranType type, char* session_id = "") = 0;
	virtual bool commit(int tran, char** conflict = 0) = 0;
	virtual void abort(int tran) = 0;

	virtual bool admin(char* s) = 0;
	virtual int request(int tran, char* s) = 0;
	virtual DbmsQuery* cursor(char* s) = 0;
	virtual DbmsQuery* query(int tran, char* s) = 0;
	virtual Lisp<gcstring> libget(char* name) = 0;
	virtual Lisp<gcstring> libraries() = 0;
	virtual Lisp<int> tranlist() = 0;
	virtual Value timestamp() = 0;
	virtual void dump(char* filename) = 0;
	virtual void copy(char* filename) = 0;
	virtual Value run(char* s) = 0;
	virtual int64 size() = 0;
	virtual Value connections() = 0;
	virtual void erase(int tran, Mmoffset recadr) = 0;
	virtual Mmoffset update(int tran, Mmoffset recadr, Record& rec) = 0;
	virtual bool record_ok(int tran, Mmoffset recadr) = 0;
	virtual Row get(Dir dir, char* query, bool one, Header& hdr, int tran = 0) = 0;
	virtual int tempdest() = 0;
	virtual int cursors() = 0;
	virtual Value sessionid(char* s) = 0;
	virtual bool refresh(int tran) = 0;
	virtual int final() = 0;
	virtual void log(char* s) = 0;
	virtual int kill(char* s) = 0;
	};

Dbms* dbms();
Dbms* dbms_local();
void set_dbms_server_ip(char* s);
char* get_dbms_server_ip();
Dbms* dbms_remote(char* server);
Dbms* dbms_remote_asynch(char* server);

bool isclient();

class Query;
class Expr;
int delete_request(int tran, Query* q);
int update_request(int tran, Query* q, const Lisp<gcstring>& c, const Lisp<Expr*>& exprs);

#endif
