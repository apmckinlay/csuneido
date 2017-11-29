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
		{ unauth(); }
	void admin(const char* s) override
		{ unauth(); }
	bool auth(const gcstring& data) override
		{ return dbms->auth(data); }
	Value check() override
		{ unauth(); }
	bool commit(int tran, const char** conflict = 0) override
		{ unauth(); }
	Value connections() override
		{ unauth(); }
	DbmsQuery* cursor(const char* s) override
		{ unauth(); }
	int cursors() override
		{ unauth(); }
	Value dump(const char* filename) override
		{ unauth(); }
	void erase(int tran, Mmoffset recadr) override
		{ unauth(); }
	Value exec(Value ob) override
		{ unauth(); }
	int final() override
		{ unauth(); }
	Row get(Dir dir, const char* query, bool one, Header& hdr, int tran = 0) override
		{ unauth(); }
	int kill(const char* s) override
		{ unauth(); }
	Lisp<gcstring> libget(const char* name) override
		{ return dbms->libget(name); }
	Lisp<gcstring> libraries() override
		{ return dbms->libraries(); }
	int load(const char* filename) override
		{ unauth(); }
	void log(const char* s) override
		{ dbms->log(s); }
	gcstring nonce() override
		{ return dbms->nonce(); }
	DbmsQuery* query(int tran, const char* s) override
		{ unauth(); }
	int readCount(int tran) override
		{ unauth(); }
	int request(int tran, const char* s) override
		{ unauth(); }
	Value run(const char* s) override
		{ unauth(); }
	Value sessionid(const char* s) override
		{ return dbms->sessionid(s); }
	int64 size() override
		{ unauth(); }
	int tempdest() override
		{ unauth(); }
	Value timestamp() override
		{ unauth(); }
	gcstring token() override
		{ unauth(); }
	Lisp<int> tranlist() override
		{ unauth(); }
	int transaction(TranType type, const char* session_id = "") override
		{ unauth(); }
	Mmoffset update(int tran, Mmoffset recadr, Record& rec) override
		{ unauth(); }
	int writeCount(int tran) override
		{ unauth(); }
private:
	Dbms* dbms;

	[[noreturn]] void unauth()
		{
		except_err("not authorized");
		}
	};

Dbms* newDbmsUnauth(Dbms* dbms)
	{
	return new DbmsUnauth(dbms);
	}
