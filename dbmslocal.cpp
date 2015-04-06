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

#include "dbms.h"
#include "thedb.h"
#include "database.h"
#include "query.h"
#include "record.h"
#include "row.h"
#include "ostreamstr.h"
#include "library.h"
#include "sudate.h"
#include "suobject.h"
#include "sustring.h"
#include "fibers.h" // for tls()
#include "auth.h"

// DbmsQueryLocal ===================================================

class DbmsQueryLocal : public DbmsQuery
	{
public:
	DbmsQueryLocal(Query* query);
	~DbmsQueryLocal()
		{ close(); }
	void set_transaction(int tran);
	Header header();
	Lisp<gcstring> order();
	Lisp<Lisp<gcstring> > keys();
	Row get(Dir dir);
    void rewind();
	void close();
	char* explain();
	bool output(const Record& rec);
private:
	Query* q;
	};

DbmsQueryLocal::DbmsQueryLocal(Query* query) : q(query)
	{ }

void DbmsQueryLocal::set_transaction(int tran)
	{
	q->set_transaction(tran);
	}

Header DbmsQueryLocal::header()
	{
	return q->header();
	}

Lisp<gcstring> DbmsQueryLocal::order()
	{
	return q->ordering();
	}

Lisp<Lisp<gcstring> > DbmsQueryLocal::keys()
	{
	return q->keys();
	}

#include "trace.h"
static Query* last_q = NULL;
void trace_last_q()
	{
	tout() << "IN: " << last_q << endl;
	}

Row DbmsQueryLocal::get(Dir dir)
	{
	last_q = q;
	Row row(q->get(dir));
	if (q->updateable() && row != Row::Eof)
		row.recadr = row.data[1].off(); // [1] to skip key
	verify(row.recadr >= 0);
	return row;
	}

void DbmsQueryLocal::rewind()
	{
	q->rewind();
	}

void DbmsQueryLocal::close()
	{
	q->close(q);
	}

char* DbmsQueryLocal::explain()
	{
	OstreamStr os;
	os << q;
	return os.str();
	}

bool DbmsQueryLocal::output(const Record& rec)
	{
	return q->output(rec);
	}

// DbmsLocal ========================================================

class DbmsLocal : public Dbms
	{
public:
	int transaction(TranType type, char* session_id = "");
	bool commit(int tran, char** conflict);
	void abort(int tran);

	bool admin(char* s);
	int request(int tran, char* s);
	DbmsQuery* cursor(char* s);
	DbmsQuery* query(int tran, char* s);
	Lisp<gcstring> libget(char* name);
	Lisp<gcstring> libraries();
	Lisp<int> tranlist();
	Value timestamp();
	void dump(char* filename);
	void copy(char* filename);
	Value run(char* s);
	int64 size();
	Value connections();
	void erase(int tran, Mmoffset recadr);
	Mmoffset update(int tran, Mmoffset recadr, Record& rec);
	bool record_ok(int tran, Mmoffset recadr);
	Row get(Dir dir, char* query, bool one, Header& hdr, int tran = 0);
	int tempdest();
	int cursors();
	Value sessionid(char* s);
	bool refresh(int tran);
	int final();
	void log(char* s);
	int kill(char* s);
	Value exec(Value ob);
	gcstring nonce();
	gcstring token();
	bool auth(const gcstring& data);
	Value check();
	};

int DbmsLocal::transaction(TranType type, char* session_id)
	{
	return theDB()->transaction((::TranType) type, session_id);
	}

bool DbmsLocal::commit(int tran, char** conflict)
	{
	return theDB()->commit(tran, conflict);
	}

void DbmsLocal::abort(int tran)
	{
	theDB()->abort(tran);
	}

bool DbmsLocal::admin(char* s)
	{
	return database_admin(s);
	}

int DbmsLocal::request(int tran, char* s)
	{
	return database_request(tran, s);
	}

DbmsQuery* DbmsLocal::cursor(char* s)
	{
	Query* q = ::query(s, IS_CURSOR);
	return new DbmsQueryLocal(q);
	}

DbmsQuery* DbmsLocal::query(int tran, char* s)
	{
	Query* q = ::query(s);
	q->set_transaction(tran);
	return new DbmsQueryLocal(q);
	}

Lisp<gcstring> DbmsLocal::libget(char* name)
	{
	return libgetall(name);
	}

Lisp<gcstring> DbmsLocal::libraries()
	{
	return ::libraries();
	}

Lisp<int> DbmsLocal::tranlist()
	{
	return theDB()->tranlist();
	}

Value DbmsLocal::timestamp()
	{
	return SuDate::timestamp();
	}

struct AutoTran
	{
	AutoTran(int t) : tin(t), tran(t <= 0 ? theDB()->transaction(READONLY) : t)
		{ }
	~AutoTran()
		{
		if (tin <= 0)
			theDB()->commit(tran);
		}
	operator int()
		{ return tran; }
	int tin;
	int tran;
	};

struct AutoQuery
	{
	AutoQuery(DbmsQuery* q_) : q(q_)
		{ }
	~AutoQuery()
		{ q->close(); }
	DbmsQuery* operator ->()
		{ return q; }
	DbmsQuery* q;
	};

Row DbmsLocal::get(Dir dir, char* querystr, bool one, Header& hdr, int t)
	{
	AutoTran tran(t);
	AutoQuery q(query(tran, querystr));
	Row row = q->get(dir);
	if (one && row != Row::Eof && q->get(dir) != Row::Eof)
		except("Query1 not unique: " << querystr);
	hdr = q->header();
	return row;
	}

#include "dump.h"

void DbmsLocal::dump(char* filename)
	{
	::dump(filename);
	}

#include "dbcopy.h"

void DbmsLocal::copy(char* filename)
	{
	db_copy(filename);
	}

extern Value run(const char*);

Value DbmsLocal::run(char* s)
	{
	return ::run(s);
	}

extern Value exec(Value ob);

Value DbmsLocal::exec(Value ob)
	{
	return ::exec(ob);
	}

int64 DbmsLocal::size()
	{
	return theDB()->mmf->size();
	}

Value DbmsLocal::connections()
	{
	extern bool is_server;
	extern SuObject& dbserver_connections();
	if (is_server)
		return &dbserver_connections();
	else
		return new SuObject(true);
	}

void DbmsLocal::erase(int tran, Mmoffset recadr)
	{
	if (! recadr)
		except("can't erase");
	void* p = theDB()->adr(recadr);
	int tblnum = ((int*) p)[-1];
	Tbl* tbl = theDB()->get_table(tblnum);
	verify(tbl);
	Record rec(theDB()->mmf, recadr);
	theDB()->remove_record(tran, tbl, rec);
	}

Mmoffset DbmsLocal::update(int tran, Mmoffset recadr, Record& newrec)
	{
	if (! recadr)
		except("can't update");
	void* p = theDB()->adr(recadr);
	int tblnum = ((int*) p)[-1];
	Tbl* tbl = theDB()->get_table(tblnum);
	verify(tbl);
	newrec.truncate(tbl->nextfield);
	Record oldrec(theDB()->mmf, recadr);
	return theDB()->update_record(tran, tbl, oldrec, newrec);
	}

bool DbmsLocal::record_ok(int tran, Mmoffset recadr)
	{
	return theDB()->record_ok(tran, recadr);
	}

int DbmsLocal::tempdest()
	{
	extern int tempdest_inuse;
	return tempdest_inuse;
	}

int DbmsLocal::cursors()
	{
	extern int cursors_inuse;
	return cursors_inuse;
	}

Value DbmsLocal::sessionid(char* s)
	{
	static char* session_id = "127.0.0.1";
	if (*s)
		session_id = dupstr(s);
	extern bool is_server;
	return new SuString(is_server ? tls().fiber_id : session_id);
	}

bool DbmsLocal::refresh(int tran)
	{
	return theDB()->refresh(tran);
	}

int DbmsLocal::final()
	{
	return theDB()->final_size();
	}

#include "errlog.h"

void DbmsLocal::log(char* s)
	{
	errlog(s);
	}

int DbmsLocal::kill(char* s)
	{
	extern bool is_server;
	extern int kill_connections(char* s); // in dbserver.cpp
	return is_server ? kill_connections(s) : 0;
	}

gcstring DbmsLocal::nonce()
	{
	except("nonce only allowed on clients");
	}

gcstring DbmsLocal::token()
	{
	return Auth::token();
	}

bool DbmsLocal::auth(const gcstring& data)
	{
	except("auth only allowed on clients");
	}

Value DbmsLocal::check()
	{
	except("check while running only supported with jSuneido server");
	}

// factory method ===================================================

Dbms* dbms_local()
	{
	return new DbmsLocal;
	}

// ==================================================================

int delete_request(int tran, Query* q)
	{
	DbmsQueryLocal dq(query_setup(q));
	dq.set_transaction(tran);
	Row row;
	int n = 0;
	for (; Row::Eof != (row = dq.get(NEXT)); ++n)
		DbmsLocal().erase(tran, row.recadr);
	return n;
	}

#include "surecord.h"
#include "qexpr.h"

int update_request(int tran, Query* q, const Fields& c, const Lisp<Expr*>& exprs)
	{
	Fields f;
	Lisp<Value> cols;
	for (f = c; ! nil(f); ++f)
		cols.push(symbol(*f));
	cols.reverse();

	q = q->transform();
	// order by key to avoid infinite loop problem
	Fields columns = q->columns();
	Fields best_key = q->key_index(columns);
	if (q->optimize(best_key, columns, Fields(), false, true) >= IMPOSSIBLE)
		except("update: invalid query");
	q = q->addindex();
	trace_tempindex(q);

	Header hdr = q->header();

	Row row;
	int n = 0;
	DbmsQueryLocal dq(q);
	dq.set_transaction(tran);
	for (; Row::Eof != (row = dq.get(NEXT)); ++n)
		{
		SuRecord* surec = row.get_surec(hdr);
		Lisp<Value> f;
		Lisp<Expr*> e;
		for (f = cols, e = exprs; ! nil(f); ++f, ++e)
			surec->putdata(*f, (*e)->eval(hdr, row));
		Record newrec = surec->to_record(hdr);
		DbmsLocal().update(tran, row.recadr, newrec);
		}
	return n;
	}
