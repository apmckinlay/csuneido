/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2017 Suneido Software Corp.
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

// the server side of the *binary* client-server protocol

#include "clientserver.h"
#include "connection.h"
#include "dbms.h"
#include "dbserverdata.h"
#include "dbmsunauth.h"
#include "auth.h"
#include "suobject.h"
#include "lisp.h"
#include "sustring.h"
#include "fibers.h"
#include "row.h"
#include "sockets.h"
#include "interp.h"
#include "sesviews.h"
#include "exceptimp.h"
#include "errlog.h"
#include "build.h"
#include "catstr.h"

// ReSharper disable CppMemberFunctionMayBeConst

#define DbServer DbServer2

class DbServer
	{
public:
	DbServer(SocketConnect* sc);
	~DbServer();

	void run();

	void cmd_ABORT();
	void cmd_ADMIN();
	void cmd_AUTH();
	void cmd_CHECK();
	void cmd_CLOSE();
	void cmd_COMMIT();
	void cmd_CONNECTIONS();
	void cmd_CURSOR();
	void cmd_CURSORS();
	void cmd_DUMP();
	void cmd_ERASE();
	void cmd_EXEC();
	void cmd_EXPLAIN();
	void cmd_FINAL();
	void cmd_GET();
	void cmd_GET1();
	void cmd_HEADER();
	void cmd_KEYS();
	void cmd_KILL();
	void cmd_LIBGET();
	void cmd_LIBRARIES();
	void cmd_LOAD();
	void cmd_LOG();
	void cmd_NONCE();
	void cmd_ORDER();
	void cmd_OUTPUT();
	void cmd_QUERY();
	void cmd_READCOUNT();
	void cmd_REQUEST();
	void cmd_REWIND();
	void cmd_RUN();
	void cmd_SESSIONID();
	void cmd_SIZE();
	void cmd_TIMESTAMP();
	void cmd_TOKEN();
	void cmd_TRANSACTION();
	void cmd_TRANSACTIONS();
	void cmd_UPDATE();
	void cmd_WRITECOUNT();

	Dbms& dbms() const
		{ return data.auth ? *::dbms() : *newDbmsUnauth(::dbms()); }
	static void abort_fn(int tran)
		{ ::dbms()->abort(tran); }
	DbmsQuery* q_or_c();
	DbmsQuery* q_or_tc();
	void putRow(const Row& row, const Header& hdr, bool sendhdr);
	Record getRecord();
	void putValue(Value val);

	void close()
		{ io.close(); }

	Connection io;
	DbServerData& data;
	SuString* session_id;
	SesViews session_views;
	Proc proc;
	};

static std::vector<DbServer*> dbservers;

static gcstring makeHello()
	{
	char* s = CATSTR3("Suneido ", build, "\r\n");
	verify(strlen(s) < HELLO_SIZE);
	gcstring h(HELLO_SIZE); // zeroed
	strcpy(h.buf(), s);
	verify(h.size() == HELLO_SIZE);
	return h;
	}
static const gcstring hello = makeHello();

DbServer::DbServer(SocketConnect* sc) : io(sc), data(*DbServerData::create())
	{
	tls().fiber_id = sc->getadr();
	session_id = new SuString(sc->getadr());
	dbserver_connections().add(session_id);
	dbservers.push_back(this);

	sc->write(hello.buf(), hello.size());

	tls().session_views = &session_views;
	tls().proc = &proc;
	}

DbServer::~DbServer()
	{
	dbservers.erase(std::remove(dbservers.begin(), dbservers.end(), this));
	dbserver_connections().remove1(session_id);
	verify(dbservers.size() == dbserver_connections().size());
	data.abort(abort_fn);
	io.close();
	}

static void _stdcall dbserver(void* sc)
	{
	try
		{
		DbServer dbs(static_cast<SocketConnect*>(sc));
		dbs.run();
		// destructor cleans up and closes socket
		}
	catch (...)
		{
		}
	Fibers::end();
	}

extern int su_port;

void start_dbserver2(char* name)
	{
	socketServer(name, su_port, dbserver, nullptr, true);
	}

typedef  void(DbServer::*CmdFn)();

// NOTE: sequence must exactly match clientserver.h and jSuneido
static CmdFn commands[] {
	&DbServer::cmd_ABORT, &DbServer::cmd_ADMIN, &DbServer::cmd_AUTH,
	&DbServer::cmd_CHECK, &DbServer::cmd_CLOSE, &DbServer::cmd_COMMIT,
	&DbServer::cmd_CONNECTIONS, &DbServer::cmd_CURSOR, &DbServer::cmd_CURSORS,
	&DbServer::cmd_DUMP, &DbServer::cmd_ERASE, &DbServer::cmd_EXEC,
	&DbServer::cmd_EXPLAIN, &DbServer::cmd_FINAL, &DbServer::cmd_GET,
	&DbServer::cmd_GET1, &DbServer::cmd_HEADER, &DbServer::cmd_KEYS,
	&DbServer::cmd_KILL, &DbServer::cmd_LIBGET, &DbServer::cmd_LIBRARIES,
	&DbServer::cmd_LOAD, &DbServer::cmd_LOG, &DbServer::cmd_NONCE,
	&DbServer::cmd_ORDER, &DbServer::cmd_OUTPUT, &DbServer::cmd_QUERY,
	&DbServer::cmd_READCOUNT, &DbServer::cmd_REQUEST, &DbServer::cmd_REWIND,
	&DbServer::cmd_RUN, &DbServer::cmd_SESSIONID, &DbServer::cmd_SIZE,
	&DbServer::cmd_TIMESTAMP, &DbServer::cmd_TOKEN, &DbServer::cmd_TRANSACTION,
	&DbServer::cmd_TRANSACTIONS, &DbServer::cmd_UPDATE, &DbServer::cmd_WRITECOUNT };

void DbServer::run()
	{
	while (true)
		{
		int c = io.getCmd();
		verify(0 <= c && c < sizeof commands / sizeof commands[0]);
		auto cmd = commands[c];
		try
			{
			(this->*cmd)();
			}
		catch (const Except& e)
			{
			io.clear();
			io.putErr(e.str());
			}
		catch (const std::exception& e)
			{
			io.clear();
			io.putErr(e.what());
			}
		catch (...)
			{
			io.clear();
			io.putErr("unknown exception");
			}
		io.write();
		}
	}

// commands ----------------------------------------------------------------------

void DbServer::cmd_ABORT()
	{
	int tn = io.getInt();
	data.end_transaction(tn);
	dbms().abort(tn);
	io.putOk();
	}

void DbServer::cmd_ADMIN()
	{
	gcstring s = io.getStr();
	dbms().admin(s.str());
	io.putOk();
	}

void DbServer::cmd_AUTH()
	{
	gcstring s = io.getStr();
	bool result = Auth::auth(data.nonce, s);
	if (result)
		data.auth = true;
	io.putOk().putBool(result);
	}

void DbServer::cmd_CHECK()
	{
	Value result = dbms().check();
	io.putOk().putStr(result.gcstr());
	}

void DbServer::cmd_CLOSE()
	{
	int n = io.getInt();
	bool ok = (io.get() == 'q')
		? data.erase_query(n)
		: data.erase_cursor(n);
	if (!ok)
		except("close failed");
	io.putOk();
	}

void DbServer::cmd_COMMIT()
	{
	int tn = io.getInt();
	data.end_transaction(tn);
	char* conflict;
	io.putOk();
	if (dbms().commit(tn, &conflict))
		io.putBool(true);
	else
		io.putBool(false).putStr(conflict);
	}

void DbServer::cmd_CONNECTIONS()
	{
	io.putOk().putValue(&dbserver_connections());
	}

void DbServer::cmd_CURSOR()
	{
	gcstring query = io.getStr();
	DbmsQuery* c = dbms().cursor(query.str());
	int cn = data.add_cursor(c);
	io.putOk().putInt(cn);
	}

void DbServer::cmd_CURSORS()
	{
	io.putOk().putInt(dbms().cursors());
	}

void DbServer::cmd_DUMP()
	{
	gcstring table = io.getStr();
	Value result = dbms().dump(table.str());
	io.putOk().putStr(result.gcstr());
	}

void DbServer::cmd_ERASE()
	{
	int tn = io.getInt();
	Mmoffset recadr = io.getInt();
	dbms().erase(tn, recadr);
	io.putOk();
	}

void DbServer::cmd_EXEC()
	{
	Value ob = io.getValue();
	Value result = dbms().exec(ob);
	putValue(result);
	}

void DbServer::cmd_EXPLAIN()
	{
	DbmsQuery* q = q_or_c();
	io.putOk().putStr(q->explain());
	}

void DbServer::cmd_FINAL()
	{
	io.putOk().putInt(dbms().final());
	}

void DbServer::cmd_GET()
	{
	Dir dir = (io.get() == '-') ? PREV : NEXT;
	DbmsQuery* q = q_or_tc();
	Row row = q->get(dir);
	Header hdr = q->header();
	return putRow(row, hdr, false);
	}

void DbServer::cmd_GET1()
	{
	char d = io.get();
	Dir dir = d == '-' ? PREV : NEXT;
	bool one = (d == '1');
	int tn = io.getInt();
	gcstring query = io.getStr();
	Header hdr;
	Row row = dbms().get(dir, query.str(), one, hdr, tn);
	return putRow(row, hdr, true);
	}

void DbServer::cmd_HEADER()
	{
	auto hdr = q_or_c()->header().schema();
	io.putOk().putStrings(hdr);
	}

void DbServer::cmd_KEYS()
	{
	auto keys = q_or_c()->keys();
	io.putOk().putInt(keys.size());
	for (auto k = keys; !nil(k); ++k)
		io.putStrings(*k);
	}

// also called by dbmslocal
int kill_connections2(char* s)
	{
	int n_killed = 0;
	for (int i = dbservers.size() - 1; i >= 0; --i) // reverse to handle erase
		try
			{
			if (dbservers[i]->session_id->gcstr() == s)
				{
				dbservers[i]->close();
				++n_killed;
				}
			}
		catch (...)
			{
			errlog("kill: error from close");
			}
	return n_killed;
	}

void DbServer::cmd_KILL()
	{
	gcstring s = io.getStr();
	int n = kill_connections2(s.str());
	io.putOk().putInt(n);
	}

void DbServer::cmd_LIBGET()
	{
	gcstring name = io.getStr();
	Lisp<gcstring> srcs = dbms().libget(name.str());
	io.putOk().putInt(srcs.size() / 2);
	for (auto src = srcs; !nil(src); ++src)
		{
		io.putStr(*src); // name
		++src;
		io.putInt(src->size()); // text size
		}
	for (auto src = srcs; !nil(src); ++src)
		{
		++src; // skip name
		io.write(src->buf(), src->size()); // text
		}
	// could mean multiple write's which is not ideal
	// but most of the time only a single definition & write
	}

void DbServer::cmd_LIBRARIES()
	{
	io.putOk().putStrings(dbms().libraries());
	}

void DbServer::cmd_LOAD()
	{
	gcstring table = io.getStr();
	int result = dbms().load(table.str());
	io.putOk().putInt(result);
	}

void DbServer::cmd_LOG()
	{
	gcstring s = io.getStr();
	dbms().log(s.str());
	io.putOk();
	}

void DbServer::cmd_NONCE()
	{
	io.putOk().putStr(dbms().nonce());
	}

void DbServer::cmd_ORDER()
	{
	DbmsQuery* q = q_or_c();
	io.putOk().putStrings(q->order());
	}

void DbServer::cmd_OUTPUT()
	{
	int qn = io.getInt();
	DbmsQuery* q = data.get_query(qn);
	Record rec = getRecord();
	q->output(rec);
	io.putOk();
	}

void DbServer::cmd_QUERY()
	{
	int tn = io.getInt();
	gcstring query = io.getStr();
	DbmsQuery* q = dbms().query(tn, query.str());
	int qn = data.add_query(tn, q);
	io.putOk().putInt(qn);
	}

void DbServer::cmd_READCOUNT()
	{
	int tn = io.getInt();
	io.putOk().putInt(dbms().readCount(tn));
	}

void DbServer::cmd_REQUEST()
	{
	int tn = io.getInt();
	gcstring s = io.getStr();
	io.putOk().putInt(dbms().request(tn, s.str()));
	}

void DbServer::cmd_REWIND()
	{
	q_or_c()->rewind();
	io.putOk();
	}

void DbServer::cmd_RUN()
	{
	gcstring s = io.getStr();
	Value result = dbms().run(s.str());
	putValue(result);
	}

void DbServer::cmd_SESSIONID()
	{
	gcstring s = io.getStr();
	if (s != "")
		{
		dbserver_connections().remove1(session_id);
		tls().fiber_id = s.str();
		session_id = new SuString(s);
		dbserver_connections().add(session_id);
		}
	io.putOk().putStr(session_id->str());
	}

void DbServer::cmd_SIZE()
	{
	io.putOk().putInt(dbms().size());
	}

void DbServer::cmd_TIMESTAMP()
	{
	io.putOk().putValue(dbms().timestamp());
	}

void DbServer::cmd_TOKEN()
	{
	io.putOk().putStr(dbms().token());
	}

void DbServer::cmd_TRANSACTION()
	{
	bool readwrite = io.getBool();
	Dbms::TranType mode = readwrite ? Dbms::READWRITE : Dbms::READONLY;
	int tn = dbms().transaction(mode, session_id->str());
	data.add_tran(tn);
	io.putOk().putInt(tn);
	}

void DbServer::cmd_TRANSACTIONS()
	{
	io.putOk().putInts(dbms().tranlist());
	}

void DbServer::cmd_UPDATE()
	{
	int tn = io.getInt();
	int recadr = io.getInt();
	Record rec = getRecord();
	recadr = dbms().update(tn, recadr, rec);
	io.putOk().putInt(recadr);
	}

void DbServer::cmd_WRITECOUNT()
	{
	int tn = io.getInt();
	io.putOk().putInt(dbms().writeCount(tn));
	}

//--------------------------------------------------------------------------------

DbmsQuery* DbServer::q_or_c()
	{
	int n = io.getInt();
	return (io.get() == 'q')
		? data.get_query(n)
		: data.get_cursor(n);
	}

DbmsQuery* DbServer::q_or_tc()
	{
	int tn = io.getInt();
	int qn = io.getInt();
	if (!isTran(tn))
		{
		DbmsQuery* q = data.get_query(qn);
		except_if(!q, "invalid query");
		return q;
		}
	else
		{
		DbmsQuery* c = data.get_cursor(qn);
		except_if(!c, "invalid cursor");
		c->set_transaction(tn);
		return c;
		}
	}

void DbServer::putRow(const Row& row, const Header& hdr, bool sendhdr)
	{
	io.putOk();
	if (nil(row.data))
		{
		io.putBool(false);
		return;
		}
	Record rec = row.to_record(hdr);
	io.putBool(true).putInt(row.recadr);
	if (sendhdr)
		io.putStrings(hdr.schema());
	io.putInt(rec.bufsize());
	io.write(static_cast<char*>(rec.ptr()), rec.bufsize());
	}

Record DbServer::getRecord()
	{
	gcstring r = io.getBuf();
	return Record(r.buf());
	}

void DbServer::putValue(Value val)
	{
	io.putOk();
	if (val)
		io.putBool(true).putValue(val);
	else
		io.putBool(false);
	}
