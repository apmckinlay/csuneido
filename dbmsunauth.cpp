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
	DbmsUnauth(Dbms* d) : dbms(d)
		{ }
	virtual int transaction(TranType type, char* session_id = "")
		{ except(notauth); }
	bool commit(int tran, char** conflict = 0) 
		{ except(notauth); }
	void abort(int tran) 
		{ except(notauth); }

	bool admin(char* s) 
		{ except(notauth); }
	int request(int tran, char* s) 
		{ except(notauth); }
	DbmsQuery* cursor(char* s) 
		{ except(notauth); }
	DbmsQuery* query(int tran, char* s) 
		{ except(notauth); }
	Lisp<gcstring> libget(char* name) 
		{ return dbms->libget(name); }
	Lisp<gcstring> libraries() 
		{ return dbms->libraries(); }
	Lisp<int> tranlist() 
		{ except(notauth); }
	Value timestamp() 
		{ except(notauth); }
	void dump(char* filename) 
		{ except(notauth); }
	void copy(char* filename) 
		{ except(notauth); }
	Value run(char* s) 
		{ except(notauth); }
	int64 size() 
		{ except(notauth); }
	Value connections() 
		{ except(notauth); }
	void erase(int tran, Mmoffset recadr) 
		{ except(notauth); }
	Mmoffset update(int tran, Mmoffset recadr, Record& rec) 
		{ except(notauth); }
	bool record_ok(int tran, Mmoffset recadr) 
		{ except(notauth); }
	Row get(Dir dir, char* query, bool one, Header& hdr, int tran = 0) 
		{ except(notauth); }
	int tempdest() 
		{ except(notauth); }
	int cursors() 
		{ except(notauth); }
	Value sessionid(char* s) 
		{ return dbms->sessionid(s); }
	bool refresh(int tran) 
		{ except(notauth); }
	int final() 
		{ except(notauth); }
	void log(char* s) 
		{ except(notauth); }
	int kill(char* s) 
		{ except(notauth); }
	Value exec(Value ob) 
		{ except(notauth); }
	gcstring nonce() 
		{ return dbms->nonce(); }
	gcstring token() 
		{ except(notauth); }
	bool auth(const gcstring& data) 
		{ return dbms->auth(data); }
	Value check()
		{ except(notauth); }
private:
	Dbms* dbms;
	const char* notauth = "not authorized";
	};

Dbms* newDbmsUnauth(Dbms* dbms)
	{
	return new DbmsUnauth(dbms);
	}