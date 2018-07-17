// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

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
		{ DbmsQueryLocal::close(); }
	void set_transaction(int tran) override;
	Header header() override;
	Lisp<gcstring> order() override;
	Lisp<Lisp<gcstring>> keys() override;
	Row get(Dir dir) override;
    void rewind() override;
	void close() override;
	const char* explain() override;
	bool output(const Record& rec) override;
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
static Query* last_q = nullptr;

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

const char* DbmsQueryLocal::explain()
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
	void abort(int tn) override;
	void admin(const char* s) override;
	bool auth(const gcstring& data) override;
	Value check() override;
	bool commit(int tn, const char** conflict) override;
	Value connections() override;
	DbmsQuery* cursor(const char* query) override;
	int cursors() override;
	Value dump(const char* filename) override;
	void erase(int tn, Mmoffset recadr) override;
	Value exec(Value ob) override;
	int final() override;
	Row get(Dir dir, const char* query, bool one, Header& hdr, int tn) override;
	int kill(const char* sessionid) override;
	Lisp<gcstring> libget(const char* name) override;
	Lisp<gcstring> libraries() override;
	int load(const char* filename) override;
	void log(const char* s) override;
	gcstring nonce() override;
	DbmsQuery* query(int tn, const char* query) override;
	int readCount(int tn) override;
	int request(int tn, const char* s) override;
	Value run(const char* s) override;
	Value sessionid(const char* s) override;
	int64_t size() override;
	int tempdest() override;
	Value timestamp() override;
	gcstring token() override;
	Lisp<int> tranlist() override;
	int transaction(TranType type, const char* session_id) override;
	Mmoffset update(int tn, Mmoffset recadr, Record& rec) override;
	int writeCount(int tn) override;
	};

int DbmsLocal::transaction(TranType type, const char* session_id)
	{
	return theDB()->transaction((::TranType) type, session_id);
	}

bool DbmsLocal::commit(int tran, const char** conflict)
	{
	return theDB()->commit(tran, conflict);
	}

void DbmsLocal::abort(int tran)
	{
	theDB()->abort(tran);
	}

void DbmsLocal::admin(const char* s)
	{
	database_admin(s);
	}

int DbmsLocal::request(int tran, const char* s)
	{
	return database_request(tran, s);
	}

DbmsQuery* DbmsLocal::cursor(const char* s)
	{
	Query* q = ::query(s, IS_CURSOR);
	return new DbmsQueryLocal(q);
	}

DbmsQuery* DbmsLocal::query(int tran, const char* s)
	{
	Query* q = ::query(s);
	q->set_transaction(tran);
	return new DbmsQueryLocal(q);
	}

Lisp<gcstring> DbmsLocal::libget(const char* name)
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
	DbmsQuery* operator ->() const
		{ return q; }
	DbmsQuery* q;
	};

Row DbmsLocal::get(Dir dir, const char* querystr, bool one, Header& hdr, int t)
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

Value DbmsLocal::dump(const char* filename)
	{
	::dump(filename);
	return SuEmptyString;
	}

#include "load.h"

int DbmsLocal::load(const char* filename)
	{
	return ::load_table(filename);
	}

extern Value run(const char*);

Value DbmsLocal::run(const char* s)
	{
	return ::run(s);
	}

extern Value exec(Value ob);

Value DbmsLocal::exec(Value ob)
	{
	return ::exec(ob);
	}

int64_t DbmsLocal::size()
	{
	return theDB()->mmf->size();
	}

extern bool is_server;
extern SuObject& dbserver_connections();

Value DbmsLocal::connections()
	{
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

extern int tempdest_inuse;

int DbmsLocal::tempdest()
	{
	return tempdest_inuse;
	}

extern int cursors_inuse;

int DbmsLocal::cursors()
	{
	return cursors_inuse;
	}

extern bool is_server;

Value DbmsLocal::sessionid(const char* s)
	{
	static const char* session_id = "127.0.0.1"; // cache
	if (*s)
		session_id = dupstr(s);
	return new SuString(is_server ? tls().fiber_id : session_id);
	}

int DbmsLocal::final()
	{
	return theDB()->final_size();
	}

#include "errlog.h"

void DbmsLocal::log(const char* s)
	{
	errlog(s);
	}

extern int kill_connections(const char* s); // in dbserver.cpp

int DbmsLocal::kill(const char* s)
	{
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

int DbmsLocal::readCount(int tran)
	{
	except("ReadCount only supported with jSuneido server");
	}

int DbmsLocal::writeCount(int tran)
	{
	except("WriteCount only supported with jSuneido server");
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

int update_request(int tran, Query* q, const Lisp<gcstring>& c, const Lisp<Expr*>& exprs)
	{
	Lisp<Value> cols;
	for (Fields f = c; ! nil(f); ++f)
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
