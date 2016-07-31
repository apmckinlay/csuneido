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

#include "dbserver.h"
#include "sockets.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <malloc.h>
#include "dbms.h"
#include "row.h"
#include "ostreamstr.h"
#include "suvalue.h"
#include "dbserverdata.h"
#include "build.h"
#include "fibers.h"
#include "interp.h" // to set proc
#include "suobject.h"
#include "gcstring.h"
#include "lisp.h"
#include "getnum.h"
#include "sesviews.h"
#include <vector>
#include "errlog.h"
#include "exceptimp.h"
#include "pack.h"
#include "trace.h"
#include "tmpalloc.h"
#include "auth.h"
#include "dbmsunauth.h"

//#define LOGGING
#ifdef LOGGING
#include "ostreamfile.h"
extern OstreamFile& dbmslog(); // in dbmsremote
#define LOG(stuff) dbmslog() << stuff << endl
#else
//#define LOG(stuff)
//#include "circlog.h"
//#define LOG(stuff) CIRCLOG((void*) this << " " << session_id << " " << stuff)
#define LOG(stuff) TRACE(CLIENTSERVER,  session_id << " " << stuff)
#endif

SuObject& dbserver_connections()
	{
	static SuObject ob(true); // readonly
	return ob;
	}

class DbServerImp : public DbServer // one instance per connection
	{
public:
	DbServerImp(SocketConnect* s);
	~DbServerImp();
	void run();
	void request(char* buf);
	void close()
		{
		sc->close();
		}
	static void timer_proc();
	char* id()
		{
		return session_id;
		}
private:
	static void abort_fn(int tran)
		{
		dbms_auth->abort(tran);
		}

	// commands
	char* cmd_text(char* s);
	char* cmd_binary(char* s);
	char* cmd_admin(char* s);
	char* cmd_cursor(char* s);
	char* cmd_close(char* s);
	char* cmd_transaction(char* s);
	char* cmd_tranlist(char* s);
	char* cmd_query(char* s);
	char* cmd_libget(char* s);
	char* cmd_libraries(char* s);
	char* cmd_request(char* s);
	char* cmd_explain(char* s);
	char* cmd_header(char* s);
	char* cmd_order(char* s);
	char* cmd_keys(char* s);
	char* cmd_get(char* s);
	char* cmd_get1(char* s);
	char* cmd_rewind(char* s);
	char* cmd_output(char* s);
	char* cmd_update(char* s);
	char* cmd_erase(char* s);
	char* cmd_commit(char* s);
	char* cmd_abort(char* s);
	char* cmd_timestamp(char* s);
	char* cmd_dump(char* s);
	char* cmd_load(char* s);
	char* cmd_run(char* s);
	char* cmd_size(char* s);
	char* cmd_connections(char* s);
	char* cmd_recordok(char* s);
	char* cmd_tempdest(char* s);
	char* cmd_cursors(char* s);
	char* cmd_sessionid(char* s);
	char* cmd_refresh(char* s);
	char* cmd_final(char*);
	char* cmd_log(char*);
	char* cmd_kill(char*);
	char* cmd_exec(char*);
	char* cmd_nonce(char*);
	char* cmd_token(char*);
	char* cmd_auth(char*);
	char* cmd_check(char*);

	char* get(DbmsQuery* q, Dir dir);

	void write(char* buf, int n)
		{ sc->write(buf, n); }
	void write(char* buf)
		{ sc->write(buf, strlen(buf)); }
	void writebuf(char* buf, int n)
		{ sc->writebuf(buf, n); }
	void writebuf(char* buf)
		{ sc->writebuf(buf, strlen(buf)); }

	DbmsQuery* q_or_c(char*& s);
	DbmsQuery* q_or_tc(char*& s);
	char* value_result(Value x);
	char* row_result(const Row& row, const Header& hdr, bool sendhdr = false);

	Dbms* dbms()
		{ return data->auth ? dbms_auth : dbms_unauth; }

	SocketConnect* sc;
	bool textmode;
	OstreamStr os;
	DbServerData* data;
	char* session_id;
	static Dbms* dbms_auth;
	static Dbms* dbms_unauth;
	int last_activity;

	Proc proc;
	SesViews session_views;
	};

static std::vector<DbServerImp*> dbservers;

const bool SEND_FIELDS = true;

Dbms* DbServerImp::dbms_auth = 0;
Dbms* DbServerImp::dbms_unauth = 0;

int su_port = 3147;
int dbserver_timeout = 240; // minutes = 4 hours

static int dbserver_clock;

static void _stdcall dbserver(void* sc)
	{
	try
		{
		DbServerImp dbs((SocketConnect*) sc);
		dbs.run();
		// destructor cleans up and closes socket
		}
	catch (...)
		{ }
	Fibers::end();
	}

static void log_once(char* s1, char* s2 = "")
	{
	static bool first = true;
	if (first)
		{
		first = false;
		errlog(s1, s2);
		}
	}

void DbServerImp::timer_proc()
	{
	try
		{
		++dbserver_clock;
		for (int i = dbservers.size() - 1; i >= 0; --i) // reverse to handle erase
			if (dbserver_clock - dbservers[i]->last_activity > dbserver_timeout)
				{
				try
					{
					errlog("idle timeout, closing connection:", dbservers[i]->session_id);
					}
				catch (...)
					{
					errlog("idle timeout, closing connection: ???");
					}
				try
					{
					dbservers[i]->close();
					}
				catch (...)
					{
					log_once("idle timeout, error from close");
					}
				}
		}
	catch (...)
		{
		log_once("idle timeout, error from iteration");
		}
	}

void start_dbserver(char* name)
	{
	socketServer(name, su_port, dbserver, 0, true);
	extern void dbserver_timer(void (*pfn)());
	dbserver_timer(DbServerImp::timer_proc);
	}

DbServerImp::DbServerImp(SocketConnect* s)
	: sc(s), textmode(true), data(DbServerData::create())
	{
	if (! dbms_auth)
		{
		dbms_auth = ::dbms();
		dbms_unauth = newDbmsUnauth(dbms_auth);
		}
	tls().fiber_id = session_id = sc->getadr();
	dbserver_connections().add(session_id);
	dbservers.push_back(this);

	os << "Suneido Database Server (" << build_date << ")\r\n";
	write(os.str());

	tls().proc = &proc;
	tls().session_views = &session_views;
	}

DbServerImp::~DbServerImp()
	{
	dbservers.erase(std::remove(dbservers.begin(), dbservers.end(), this));
	dbserver_connections().remove1(session_id);
	verify(dbservers.size() == dbserver_connections().size());
	data->abort(abort_fn);
	sc->close();
	}

void DbServerImp::run()
	{
	char buf[32000];
	while (sc->readline(buf, sizeof buf))
		request(buf);
	}

inline bool match(char* s, char* pre)
	{
	const int npre = strlen(pre);
	return 0 == _memicmp(s, pre, npre) &&
		(s[npre] == ' ' || s[npre] == 0);
	}

struct Cmd
	{
	char* cmd;
	char* (DbServerImp::*fn)(char* buf);
	};

#define CMD(name)	{ #name, &DbServerImp::cmd_##name }

void DbServerImp::request(char* buf)
	{
	static Cmd cmds[] =
		{ // most frequently used first
		CMD(libget),
		CMD(get),
		CMD(get1),
		CMD(output),
		CMD(update),
		CMD(header),
		CMD(order),
		CMD(keys),
		CMD(transaction),
		CMD(cursor),
		CMD(close),
		CMD(query),
		CMD(request),
		CMD(admin),
		CMD(libraries),
		CMD(explain),
		CMD(rewind),
		CMD(erase),
		CMD(commit),
		CMD(abort),
		CMD(timestamp),
		CMD(dump),
		CMD(load),
		CMD(run),
		CMD(text),
		CMD(binary),
		CMD(tranlist),
		CMD(size),
		CMD(connections),
		CMD(recordok),
		CMD(tempdest),
		CMD(cursors),
		CMD(sessionid),
		CMD(refresh),
		CMD(final),
		CMD(log),
		CMD(kill),
		CMD(exec),
		CMD(nonce),
		CMD(token),
		CMD(auth),
		CMD(check)
		};
	const int ncmds = sizeof cmds / sizeof (Cmd);

	LOG("s< " << buf);
	last_activity = dbserver_clock;
	os.clear();
	int n = strlen(buf);
	while (n > 0 && isspace(buf[n - 1]))
		buf[--n] = 0;
	if (n == 0)
		return ;
	for (int i = 0; i < ncmds; ++i)
		if (match(buf, cmds[i].cmd))
			{
			try
				{
				char* s = (this->*(cmds[i].fn))(buf + strlen(cmds[i].cmd) + 1);
				if (s)
					{
					LOG("s> " << s);
					write(s);
					}
				}
			catch (const Except& e)
				{
				os.clear();
				char* t = dupstr(e.str());
				for (char* s = t; *s; ++s)
					if (*s == '\r')
						*s = '\\';
					else if (*s == '\n')
						*s = 'n';
				os << "ERR " << t << "\r\n";
				LOG("s> " << os.str());
				write(os.str());
				}
			catch (const std::exception& e)
				{
				os.clear();
				os << "ERR " << e.what() << "\r\n";
				write(os.str());
				}
			catch (...)
				{
				write("ERR unknown exception\r\n");
				}
			return ;
			}
	os << "ERR no such command: " << buf << "\r\n";
	write(os.str());
	}

static char* bool_result(bool result)
	{
	return (char*) (result ? "t\r\n" : "f\r\n");
	}

char* DbServerImp::cmd_text(char* req)
	{
	textmode = true;
	return "OK\r\n";
	}

char* DbServerImp::cmd_binary(char* req)
	{
	textmode = false;
	return "OK\r\n";
	}

char* DbServerImp::cmd_admin(char* s)
	{
	return bool_result(dbms()->admin(s));
	}

char* DbServerImp::cmd_cursor(char* s)
	{
	int qlen = ck_getnum('Q', s);
	char* buf = tmpalloc(qlen + 1);
	sc->read(buf, qlen);
	buf[qlen] = 0;
	DbmsQuery* q = dbms()->cursor(buf);
	os << 'C' << data->add_cursor(q) << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_close(char* s)
	{
	int n;
	bool ok = false;
	if (ERR != (n = getnum('Q', s)))
		ok = data->erase_query(n);
	else if (ERR != (n = getnum('C', s)))
		ok = data->erase_cursor(n);
	return (char*) (ok ? "OK\r\n" : "ERR invalid CLOSE\r\n");
	}

char* DbServerImp::cmd_transaction(char* s)
	{
	Dbms::TranType mode;
	if (match(s, "read"))
		mode = Dbms::READONLY;
	else if (match(s, "update"))
		mode = Dbms::READWRITE;
	else
		return "ERR invalid mode\r\n";
	int tran = dbms()->transaction(mode, session_id);
	data->add_tran(tran);
	os << 'T' << tran << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_request(char* s)
	{
	int tran = ck_getnum('T', s);
	if (! textmode)
		{ // binary
		int qlen = ck_getnum('Q', s);
		char* buf = tmpalloc(qlen + 1);
		sc->read(buf, qlen);
		buf[qlen] = 0;
		LOG("q: " << buf << endl);
		s = buf;
		}
	os << 'R' << dbms()->request(tran, s) << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_query(char* s)
	{
	int tran = ck_getnum('T', s);
	int qlen = ck_getnum('Q', s);
	char* buf = tmpalloc(qlen + 1);
	sc->read(buf, qlen);
	buf[qlen] = 0;
	LOG("q: " << buf << endl);
	DbmsQuery* q = dbms()->query(tran, buf);
	os << 'Q' << data->add_query(tran, q) << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_libget(char* name)
	{
	Lisp<gcstring> srcs = dbms()->libget(name);

	Lisp<gcstring> s;
	for (s = srcs; ! nil(s); ++s)
		{
		++s; // skip library name
		os << 'L' << s->size() << ' ';
		}
	os << "\r\n";
	writebuf(os.str());
	for (s = srcs; ! nil(s); ++s)
		{
		os.clear();
		os << *s << "\r\n";
		++s;
		writebuf(os.str());
		writebuf(s->buf(), s->size());
		}
	write("");
	return 0;
	}

char* DbServerImp::cmd_libraries(char*)
	{
	os << dbms()->libraries() << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_tranlist(char*)
	{
	os << data->get_trans() << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_size(char*)
	{
	os << 'S' << mmoffset_to_int(dbms()->size()) << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_tempdest(char*)
	{
	os << 'D' << dbms()->tempdest() << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_cursors(char*)
	{
	os << 'N' << dbms()->cursors() << "\r\n";
	return os.str();
	}

char* DbServerImp::value_result(Value x)
	{
	if (! x)
		return "\r\n";
	int n = x.packsize();
	os << 'P' << n << "\r\n";
	writebuf(os.str());
	char* buf = tmpalloc(n);
	x.pack(buf);
	write(buf, n);
	return 0;
	}

char* DbServerImp::cmd_connections(char*)
	{
	return value_result(&dbserver_connections());
	}

char* DbServerImp::cmd_sessionid(char* s)
	{
	if (*s)
		{
		dbserver_connections().remove1(session_id);
		tls().fiber_id = session_id = dupstr(s);
		dbserver_connections().add(session_id);
		}
	os << session_id << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_refresh(char* s)
	{
	int tran = ck_getnum('T', s);
	return bool_result(dbms()->refresh(tran));
	}

char* DbServerImp::cmd_final(char*)
	{
	os << 'N' << dbms()->final() << "\r\n";
	return os.str();
	}

DbmsQuery* DbServerImp::q_or_c(char*& s)
	{
	DbmsQuery* q = 0;
	int n;
	if (ERR != (n = getnum('Q', s)))
		q = data->get_query(n);
	else if (ERR != (n = getnum('C', s)))
		q = data->get_cursor(n);
	if (! q)
		except("valid query or cursor required");
	return q;
	}

char* DbServerImp::cmd_explain(char* s)
	{
	DbmsQuery* q = q_or_c(s);
	os << q->explain() << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_header(char* s)
	{
	DbmsQuery* q = q_or_c(s);
	os << q->header().schema() << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_order(char* s)
	{
	DbmsQuery* q = q_or_c(s);
	os << q->order() << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_keys(char* s)
	{
	getnum('T', s); // not used
	DbmsQuery* q = q_or_c(s);
	os << q->keys() << "\r\n";
	return os.str();
	}

DbmsQuery* DbServerImp::q_or_tc(char*& s)
	{
	DbmsQuery* q = 0;
	int n, t;
	if (ERR != (n = getnum('Q', s)))
		q = data->get_query(n);
	else if (ERR != (t = getnum('T', s)) && ERR != (n = getnum('C', s)))
		{
		q = data->get_cursor(n);
		if (q)
			q->set_transaction(t);
		}
	if (! q)
		except("valid query or transaction & cursor required");
	return q;
	}

char* DbServerImp::cmd_get(char* s)
	{
	Dir dir;
	if (*s == '+')
		dir = NEXT;
	else if (*s == '-')
		dir = PREV;
	else
		return "ERR GET requires + or -";
	s += 2;

	DbmsQuery* q = q_or_tc(s);
	return get(q, dir);
	}

char* DbServerImp::get(DbmsQuery* q, Dir dir)
	{
	Row row = q->get(dir);
	Header hdr = q->header();
	return row_result(row, hdr);
	}

static int deletedSize(const Row& row, const Header& hdr)
	{
	int deleted = 0;
	Lisp<Record> data = row.data;
	Lisp<Fields> flds = hdr.flds;
	for (; ! nil(data); ++data, ++flds)
		{
		int i = 0;
		for (Fields f = *flds; ! nil(f); ++f, ++i)
			if (*f == "-")
				deleted += data->getraw(i).size();
		}
	return deleted;
	}

const int SMALL_RECORD = 1024;
const int HUGE_RECORD = 256 * 1024; // safe to alloca

static bool shouldRebuild(const Row& row, const Header& hdr, const Record& rec)
	{
	if (row.data.size() > 2)
		return true; // must rebuild
	if (rec.cursize() < SMALL_RECORD)
		return false;
	return deletedSize(row, hdr) > rec.cursize() / 3;
	}

static bool shouldCompact(const Record& rec)
	{
	if (rec.cursize() < SMALL_RECORD)
		return false;
	if (rec.cursize() > HUGE_RECORD)
		return false; // too big to alloca
	return (rec.bufsize() - rec.cursize()) > rec.cursize() / 3;
	}

char* DbServerImp::row_result(const Row& row, const Header& hdr, bool sendhdr)
	{
	if (nil(row.data))
		return "EOF\r\n";
	if (textmode)
		{
		os << '(';
		bool first = true;
		for (Fields f = hdr.fields(); ! nil(f); ++f)
			{
			if (! first)
				os << ", ";
			first = false;
			os << *f << ": " << row.getval(hdr, *f);
			}
		os << ")\r\n";
		return os.str();
		}
	else // binary
		{
		Record rec;
		if (row.data.size() == 1)
			rec = row.data[0];
		else if (row.data.size() == 2)
			rec = row.data[1];
		if (shouldRebuild(row, hdr, rec))
			{
			rec = Record(1000);
			for (Fields f = hdr.fields(); ! nil(f); ++f)
				rec.addraw(row.getraw(hdr, *f));

			// strip trailing empty fields
			int n = rec.size();
			while (rec.getraw(n - 1).size() == 0)
				--n;
			rec.truncate(n);
			}
		if (shouldCompact(rec))
			rec = rec.dup();

		os << 'A' << mmoffset_to_int(row.recadr) << " R" << rec.bufsize();
		if (sendhdr)
			os << ' ' << hdr.schema();
		os << "\r\n";
		LOG("s> " << os.str());
		writebuf(os.str());
		
		write((char*) rec.ptr(), rec.bufsize());
		return 0;
		}
	}

char* DbServerImp::cmd_get1(char* s)
	{
	Dir dir = NEXT;
	bool one = false;
	if (*s == '+')
		{}
	else if (*s == '-')
		dir = PREV;
	else if (*s == '1')
		one = true;
	else
		return "ERR GET requires + or -";
	s += 2;

	int tran = ck_getnum('T', s);
	int qlen = ck_getnum('Q', s);
	char* buf = tmpalloc(qlen + 1);
	sc->read(buf, qlen);
	buf[qlen] = 0;
	LOG("q: " << buf << endl);

	Header hdr;
	Row row = dbms()->get(dir, buf, one, hdr, tran);
	return row_result(row, hdr, true);
	}

char* DbServerImp::cmd_rewind(char* s)
	{
	getnum('T', s); // not used
	DbmsQuery* q = q_or_c(s);
	q->rewind();
	return "OK\r\n";
	}

char* DbServerImp::cmd_output(char* s)
	{
	DbmsQuery* q = q_or_tc(s);

	int reclen = ck_getnum('R', s);

	char* buf = tmpalloc(reclen);
	sc->read(buf, reclen);
	Record rec((void*) buf);
	return bool_result(q->output(rec));
	}

char* DbServerImp::cmd_erase(char* s)
	{
	int tran = ck_getnum('T', s);
	Mmoffset recadr = int_to_mmoffset(ck_getnum('A', s));
	dbms()->erase(tran, recadr);
	return "OK\r\n";
	}

char* DbServerImp::cmd_update(char* s)
	{
	int tran = ck_getnum('T', s);
	Mmoffset recadr = int_to_mmoffset(ck_getnum('A', s));
	verify(recadr >= 0);
	int reclen = ck_getnum('R', s);
	char* buf = tmpalloc(reclen);
	sc->read(buf, reclen);
	Record newrec((void*) buf);

	os << 'U' << mmoffset_to_int(dbms()->update(tran, recadr, newrec)) << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_recordok(char* s)
	{
	int tran = ck_getnum('T', s);
	Mmoffset recadr = int_to_mmoffset(ck_getnum('A', s));
	return bool_result(dbms()->record_ok(tran, recadr));
	}

char* DbServerImp::cmd_commit(char* s)
	{
	int tran = ck_getnum('T', s);
	data->end_transaction(tran);
	char* conflict;
	if (dbms()->commit(tran, &conflict))
		return "OK\r\n";
	else
		{
		os << conflict << "\r\n";
		return os.str();
		}
	}

char* DbServerImp::cmd_abort(char* s)
	{
	int tran = ck_getnum('T', s);
	data->end_transaction(tran);
	dbms()->abort(tran);
	return "OK\r\n";
	}

char* DbServerImp::cmd_timestamp(char* s)
	{
	os << dbms()->timestamp() << "\r\n";
	return os.str();
	}

#include "dump.h"

char* DbServerImp::cmd_dump(char* s)
	{
	dbms()->dump(s);
	return value_result("");
	}

#include "load.h"

char* DbServerImp::cmd_load(char* s)
	{
	os << 'N' << dbms()->load(s) << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_run(char* s)
	{
	Value x = dbms()->run(s);
	if (! x)
		return "\r\n";
	if (textmode)
		{
		os << x << "\r\n";
		return os.str();
		}
	else // binary
		{
		return value_result(x);
		}
	}

extern Value exec(Value ob);

char* DbServerImp::cmd_exec(char* s)
	{
	int n = ck_getnum('P', s);
	gcstring buf(n);
	sc->read(buf.str(), n);
	return value_result(exec(unpack(buf)));
	}

char* DbServerImp::cmd_log(char* s)
	{
	errlog(session_id, s);
	return "OK\r\n";
	}

static bool matches(int i, char* sid)
	{
	try
		{
		return 0 == strcmp(dbservers[i]->id(), sid);
		}
	catch (...)
		{
		errlog("kill: bad entry in dbservers");
		}
	return false;
	}

int kill_connections(char* s)
	{
	int n_killed = 0;
	for (int i = dbservers.size() - 1; i >= 0; --i) // reverse to handle erase
		if (matches(i, s))
			try
				{
				dbservers[i]->close();
				++n_killed;
				}
			catch (...)
				{
				errlog("kill: error from close");
				}
	return n_killed;
	}

char* DbServerImp::cmd_kill(char* s)
	{
	int n_killed = kill_connections(s);
	os << 'N' << n_killed << "\r\n";
	return os.str();
	}

char* DbServerImp::cmd_nonce(char* s)
	{
	gcstring nonce = Auth::nonce();
	write(nonce.buf(), nonce.size());
	data->nonce = nonce;
	return 0;
	}

char* DbServerImp::cmd_token(char* s)
	{
	gcstring token = Auth::token();
	write(token.buf(), token.size());
	return 0;
	}

char* DbServerImp::cmd_auth(char* s)
	{
	int n = ck_getnum('D', s);
	gcstring buf(n);
	sc->read(buf.buf(), n);
	bool result = Auth::auth(data->nonce, buf);
	if (result)
		data->auth = true;
	return  bool_result(result);
	}

char* DbServerImp::cmd_check(char* s)
	{
	return value_result(dbms()->check());
	}