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
#include "fibers.h"

static char* server_ip = 0; // local

void set_dbms_server_ip(char* s)
	{
	server_ip = s;
	}

char* get_dbms_server_ip()
	{
	return server_ip;
	}

Dbms* dbms()
	{
	if (server_ip)
		{ // client
		if (! tls().thedbms)
			{
			if (Fibers::inMain())
				tls().thedbms = dbms_remote(server_ip);
			else
				{
				tls().thedbms = dbms_remote_async(server_ip);
				tls().thedbms->auth(Fibers::main_dbms()->token());
				}
			}
		return tls().thedbms;
		}
	else
		{
		static Dbms* local = dbms_local();
		return local;
		}
	}

bool isclient()
	{ return server_ip != 0; }

// tests ============================================================

#include "testing.h"
#include "tempdb.h"
#include "row.h"
#include "value.h"

class test_dbms : public Tests
	{
	TEST(1, recadr)
		{
		TempDB tempdb;

		int tran;

		// so trigger searches won't give error
		dbms()->admin("create stdlib (group, name, text) key(name,group)");

		// create customer file
		dbms()->admin("create customer (id, name, city) key(id)");
		tran = dbms()->transaction(Dbms::READWRITE);
		dbms()->request(tran, "insert {id: \"a\", name: \"axon\", city: \"saskatoon\"} into customer");
		dbms()->request(tran, "insert {id: \"b\", name: \"bob\", city: \"regina\"} into customer");
		verify(dbms()->commit(tran));

		{ tran = dbms()->transaction(Dbms::READWRITE);
		DbmsQuery* q = dbms()->query(tran, "customer");
		Row row = q->get(NEXT);
		verify(row.recadr);
		dbms()->erase(tran, row.recadr);
		verify(dbms()->commit(tran)); }

		{ tran = dbms()->transaction(Dbms::READWRITE);
		DbmsQuery* q = dbms()->query(tran, "customer");
		Row row = q->get(NEXT);
		verify(row.recadr);
		Record rec;
		rec.addval("b");
		rec.addval("Bob");
		rec.addval("Regina");
		dbms()->update(tran, row.recadr, rec);
		verify(dbms()->commit(tran)); }

		{ tran = dbms()->transaction(Dbms::READONLY);
		DbmsQuery* q = dbms()->query(tran, "customer");
		Row row = q->get(NEXT);
		Record rec = row.data[1];
		assert_eq(rec.getval(1), Value("Bob"));
		verify(dbms()->commit(tran)); }
		}
	};
REGISTER(test_dbms);

void dbms_test()
	{
	int tran;

	// create customer file
	try { dbms()->admin("drop dbms_test"); } catch (...) { }
	dbms()->admin("create dbms_test (id, name, city) key(id)");
	tran = dbms()->transaction(Dbms::READWRITE);
	dbms()->request(tran, "insert {id: \"a\", name: \"axon\", city: \"saskatoon\"} into dbms_test");
	dbms()->request(tran, "insert {id: \"b\", name: \"bob\", city: \"regina\"} into dbms_test");
	verify(dbms()->commit(tran));

	{ tran = dbms()->transaction(Dbms::READWRITE);
	DbmsQuery* q = dbms()->query(tran, "dbms_test");
	Row row = q->get(NEXT);
	verify(row.recadr);
	dbms()->erase(tran, row.recadr);
	verify(dbms()->commit(tran)); }

	{ tran = dbms()->transaction(Dbms::READWRITE);
	DbmsQuery* q = dbms()->query(tran, "dbms_test");
	Row row = q->get(NEXT);
	verify(row.recadr);
	Record rec;
	rec.addval("b");
	rec.addval("Bob");
	rec.addval("Regina");
	dbms()->update(tran, row.recadr, rec);
	verify(dbms()->commit(tran)); }

	{ tran = dbms()->transaction(Dbms::READONLY);
	DbmsQuery* q = dbms()->query(tran, "dbms_test");
	Row row = q->get(NEXT);
	Record rec = row.data[1];
	assert_eq(rec.getval(1), Value("Bob"));
	verify(dbms()->commit(tran)); }

	dbms()->admin("drop dbms_test");
	}
