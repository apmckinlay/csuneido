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
#include "gcstring.h"
#include "fibers.h"
#include "exceptimp.h"

static const char* server_ip = 0; // local

void set_dbms_server_ip(const char* s)
	{
	server_ip = s;
	}

const char* get_dbms_server_ip()
	{
	return server_ip;
	}

bool isclient()
	{
	return server_ip != nullptr;
	}

Dbms* dbms()
	{
	if (isclient())
		{
		if (! tls().thedbms)
			{
			if (Fibers::inMain())
				tls().thedbms = dbms_remote(server_ip);
			else
				{
				try
					{
					tls().thedbms = dbms_remote_async(server_ip);
					}
				catch (const Except& e)
					{
					throw Except(e, 
						"thread failed to connect to db server: " + e.gcstr());
					}
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

// tests ------------------------------------------------------------

#include "testing.h"
#include "tempdb.h"
#include "row.h"
#include "value.h"

TEST(dbms_recadr)
	{
	TempDB tempdb;

	// so trigger searches won't give error
	dbms()->admin("create stdlib (group, name, text) key(name,group)");

	// create customer file
	dbms()->admin("create customer (id, name, city) key(id)");
	int tran = dbms()->transaction(Dbms::READWRITE);
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
