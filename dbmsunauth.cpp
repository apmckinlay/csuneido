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
#include "lisp.h"
#include "gcstring.h"
#include "row.h"
#include "value.h"

class DbmsUnauth : public Dbms
	{
public:
	explicit DbmsUnauth(Dbms* d) : dbms(d)
		{ }

	void abort(int tran) override
		{ except(notauth); }
	void admin(const char* s) override
		{ except(notauth); }
	bool auth(const gcstring& data) override
		{ return dbms->auth(data); }
	Value check() override
		{ except(notauth); }
	bool commit(int tran, const char** conflict = 0) override
		{ except(notauth); }
	Value connections() override
		{ except(notauth); }
	DbmsQuery* cursor(const char* s) override
		{ except(notauth); }
	int cursors() override
		{ except(notauth); }
	Value dump(const char* filename) override
		{ except(notauth); }
	void erase(int tran, Mmoffset recadr) override
		{ except(notauth); }
	Value exec(Value ob) override
		{ except(notauth); }
	int final() override
		{ except(notauth); }
	Row get(Dir dir, const char* query, bool one, Header& hdr, int tran = 0) override
		{ except(notauth); }
	int kill(const char* s) override
		{ except(notauth); }
	Lisp<gcstring> libget(const char* name) override
		{ return dbms->libget(name); }
	Lisp<gcstring> libraries() override
		{ return dbms->libraries(); }
	int load(const char* filename) override
		{ except(notauth); }
	void log(const char* s) override
		{ except(notauth); }
	gcstring nonce() override
		{ return dbms->nonce(); }
	DbmsQuery* query(int tran, const char* s) override
		{ except(notauth); }
	int readCount(int tran) override
		{ except(notauth); }
	int request(int tran, const char* s) override
		{ except(notauth); }
	Value run(const char* s) override
		{ except(notauth); }
	Value sessionid(const char* s) override
		{ return dbms->sessionid(s); }
	int64 size() override
		{ except(notauth); }
	int tempdest() override
		{ except(notauth); }
	Value timestamp() override
		{ except(notauth); }
	gcstring token() override
		{ except(notauth); }
	Lisp<int> tranlist() override
		{ except(notauth); }
	int transaction(TranType type, const char* session_id = "") override
		{ except(notauth); }
	Mmoffset update(int tran, Mmoffset recadr, Record& rec) override
		{ except(notauth); }
	int writeCount(int tran) override
		{ except(notauth); }
private:
	Dbms* dbms;
	const char* notauth = "not authorized";
	};

Dbms* newDbmsUnauth(Dbms* dbms)
	{
	return new DbmsUnauth(dbms);
	}
