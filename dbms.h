#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "dir.h"
#include "mmoffset.h"

// Dbms - pure virtual interface class for Database and Query packages

const int NO_TRAN = 0;
inline bool isTran(int tn) {
	return tn > 0;
} // handle both -1 and 0

class Record;
class Row;
class Header;
template <class T>
class Lisp;
class gcstring;
class SuValue;

class DbmsQuery {
public:
	virtual ~DbmsQuery() = default;
	virtual Header header() = 0;
	virtual Lisp<gcstring> order() = 0;
	virtual Lisp<Lisp<gcstring> > keys() = 0;
	virtual Row get(Dir dir) = 0;
	virtual void rewind() = 0;
	virtual const char* explain() = 0;
	virtual bool output(Record rec) = 0;
	virtual void set_transaction(int tn) = 0;
	virtual void close() = 0;
};

class Value;

class Dbms {
public:
	virtual ~Dbms() = default;

	enum TranType { READONLY, READWRITE };
	virtual void abort(int tn) = 0;
	virtual void admin(const char* s) = 0;
	virtual bool auth(const gcstring& data) = 0;
	virtual Value check() = 0;
	virtual bool commit(int tn, const char** conflict = 0) = 0;
	virtual Value connections() = 0;
	virtual DbmsQuery* cursor(const char* query) = 0;
	virtual int cursors() = 0;
	virtual Value dump(const char* filename) = 0;
	virtual void erase(int tn, Mmoffset recadr) = 0;
	virtual Value exec(Value ob) = 0;
	virtual int final() = 0;
	virtual Row get(Dir dir, const char* query, bool one, Header& hdr,
		int tn = NO_TRAN) = 0;
	virtual int kill(const char* sessionid) = 0;
	virtual Lisp<gcstring> libget(const char* name) = 0;
	virtual Lisp<gcstring> libraries() = 0;
	virtual int load(const char* filename) = 0;
	virtual void log(const char* s) = 0;
	virtual gcstring nonce() = 0;
	virtual DbmsQuery* query(int tn, const char* query) = 0;
	virtual int readCount(int tn) = 0;
	virtual int request(int tn, const char* s) = 0;
	virtual Value run(const char* s) = 0;
	virtual Value sessionid(const char* s) = 0;
	virtual int64_t size() = 0;
	virtual int tempdest() = 0;
	virtual Value timestamp() = 0;
	virtual gcstring token() = 0;
	virtual Lisp<int> tranlist() = 0;
	virtual int transaction(TranType type, const char* session_id = "") = 0;
	virtual Mmoffset update(int tn, Mmoffset recadr, Record& rec) = 0;
	virtual int writeCount(int tn) = 0;
};

Dbms* dbms();
Dbms* dbms_local();
void set_dbms_server_ip(const char* s);
const char* get_dbms_server_ip();
Dbms* dbms_remote(const char* server);
Dbms* dbms_remote_async(const char* server);
Dbms* dbms_remote2(const char* server);
Dbms* dbms_remote2_async(const char* server);

bool isclient();

class Query;
class Expr;
int delete_request(int tn, Query* q);
int update_request(
	int tn, Query* q, const Lisp<gcstring>& c, const Lisp<Expr*>& exprs);
