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
#include "record.h"
#include "row.h"
#include "ostreamstr.h"
#include "sockets.h"
#include "build.h"
#include <stdlib.h>
#include <ctype.h>
#include "lisp.h"
#include "gcstring.h"
#include "sudate.h"
#include "pack.h"
#include "fatal.h"
#include "getnum.h"
#include "value.h"
#include "sustring.h"
#include "gc.h"
#include "cmdlineoptions.h" // for ignore_version
#include "exceptimp.h"
#include "tmpalloc.h"
#include "fibers.h" // for tls()
#include "auth.h" // for NONCE_SIZE and TOKEN_SIZE
#include "tr.h"

//#define LOGGING
#ifdef LOGGING
#include "ostreamfile.h"
OstreamFile& dbmslog()
	{
	static OstreamFile log("dbms.log", "ab");
	return log;
	}
#define LOG(stuff) dbmslog() << stuff << endl
#else
//#define LOG(stuff)
//#include "circlog.h"
//#define LOG(stuff) CIRCLOG(stuff)
#include "trace.h"
#define LOG(stuff) TRACE(CLIENTSERVER, stuff)
#endif

#define DO(fn) try { fn; } catch (const Except& e) { fatal("lost connection:", e.str()); exit(0); }

#define WRITE_LIMIT	(1024 * 1024) // 1 mb
#define CK(n) except_if(n > WRITE_LIMIT, "client write of " << n << " exceeds limit of " << WRITE_LIMIT)

class CheckedSocketConnect
	{
public:
	CheckedSocketConnect(SocketConnect* s) : sc(s)
		{ }
	void write(char* s)
		{ DO(sc->write(s)); }
	void write(char* buf, int len)
		{ DO(sc->write(buf, len)); }
	void writebuf(char* s)
		{ DO(sc->writebuf(s)); }
	void writebuf(char* buf, int len)
		{ DO(sc->writebuf(buf, len)); }
	void read(char* buf, int len)
		{ DO(if (len != sc->read(buf, len)) except("socket read timeout in dbmsremote")); }
	void readline(char* buf, int len)
		{ DO(if (! sc->readline(buf, len)) except("socket readline timeout in dbmsremote")); }
	char* ck_readline(char* buf, int len)
		{
		readline(buf, len);
		LOG("c< " << buf);
		if (has_prefix(buf, "ERR "))
			except((buf + 4) << " (from server)");
		return buf;
		}
	enum { BUFSIZE = 500 }; // large enough for exceptions
	void ck_ok()
		{
		char buf[BUFSIZE];
		ck_readline(buf, sizeof buf);
		if (0 != strcmp(buf, "OK\r\n"))
			except("expected 'OK', got: " << buf);
		}
	bool readbool()
		{
		char buf[BUFSIZE];
		ck_readline(buf, sizeof buf);
		if (*buf != 't' && *buf != 'f')
			except("expected 't' or 'f', got: " << buf);
		return *buf == 't';
		}
	int readint(char c)
		{
		char buf[BUFSIZE];
		ck_readline(buf, sizeof buf);
		char* s = buf;
		return ck_getnum(c, s);
		}
	Value readvalue()
		{
		char buf[BUFSIZE];
		ck_readline(buf, sizeof buf);
		if (0 == strcmp(buf, "\r\n"))
			return Value();
		char* s = buf;
		int n = ck_getnum('P', s);
		gcstring result(n);
		read(result.str(), n);
		return unpack(result);
		}
	void close()
		{ sc->close(); }
	SocketConnect* sc;
	};

#define WRITE(stuff) \
	do { \
	os.clear(); \
	os << stuff << "\r\n"; \
	LOG("c> " << os.str()); \
	sc.write(os.str()); \
	} while(0)
#define WRITEBUF(stuff) \
	do { \
	os.clear(); \
	os << stuff << "\r\n"; \
	LOG("c> " << os.str()); \
	sc.writebuf(os.str()); \
	} while(0)

// DbmsQueryRemote ===================================================

class DbmsQueryRemote : public DbmsQuery
	{
public:
	DbmsQueryRemote(CheckedSocketConnect& sc, int q);
	void set_transaction(int tran);
	Header header();
	Lisp<gcstring> order();
	Lisp<Lisp<gcstring> > keys();
	Row get(Dir dir);
	void rewind();
	void close();
	char* explain();
	bool output(const Record& rec);
	virtual void out(Ostream& os);
protected:
	CheckedSocketConnect& sc;
	int id;
	Header header_;
	Lisp<Lisp<gcstring> > keys_;
	OstreamStr os;
	};

Ostream& operator<<(Ostream& os, DbmsQueryRemote* q)
	{ q->out(os); return os; }

DbmsQueryRemote::DbmsQueryRemote(CheckedSocketConnect& s, int i) : sc(s), id(i)
	{ }

void DbmsQueryRemote::set_transaction(int tran)
	{ }

static Header s_to_header(char* s)
	{
	if (s[0] != '(' || s[strlen(s) - 3] != ')')
		except("HEADER expected (...)\r\ngot: " << s);
	Lisp<gcstring> cols;
	Lisp<gcstring> fields;
	for (++s; *s != '\r'; )
		{
		int n = strcspn(s, ",)");
		if (isupper(*s))
			*s = tolower(*s);
		else
			fields.push(gcstring(s, n));
		if (n != 1 || *s != '-')
			cols.push(gcstring(s, n));
		s += n + 1;
		}
	cols.reverse();
	fields.reverse();
	return Header(lisp(fields), cols);
	}

const int MAXHDRSIZE = 16000;

Header DbmsQueryRemote::header()
	{
	if (nil(header_))
		{
		WRITE("HEADER " << this);
		char buf[MAXHDRSIZE];
		sc.ck_readline(buf, sizeof buf);
		header_ = s_to_header(buf);
		}
	return header_;
	}

Lisp<gcstring> DbmsQueryRemote::order()
	{
	WRITE("ORDER " << this);
	char buf[1000];
	sc.ck_readline(buf, sizeof buf);
	if (buf[0] != '(' || buf[strlen(buf) - 3] != ')')
		except("ORDER expected (...)\r\ngot: " << buf);
	Lisp<gcstring> cols;
	for (char* s = buf + 1; *s != '\r'; )
		{
		int n = strcspn(s, ",)");
		if (n > 0)
			cols.push(gcstring(s, n));
		s += n + 1;
		}
	return cols.reverse();
	}

Lisp<Lisp<gcstring> > DbmsQueryRemote::keys()
	{
	if (nil(keys_))
		{
		WRITE("KEYS " << this);
		char buf[MAXHDRSIZE];
		sc.ck_readline(buf, sizeof buf);
		if (buf[0] != '(' || buf[strlen(buf) - 3] != ')')
			except("KEYS expected (...)\r\ngot: " << buf);
		for (char* s = buf + 1; *s != '\r'; )
			{
			Lisp<gcstring> fields;
			++s;
			if (*s == ')')
				++s;
			else
				while (*s != ',' && *s != ')')
					{
					int n = strcspn(s, ",)");
					fields.push(gcstring(s, n));
					s += n + 1; // skip field + comma or )
					}
			fields.reverse();
			keys_.push(fields);
			++s; // skip comma or )
			}
		keys_.reverse();
		}
	return keys_;
	}

static Row getrec(CheckedSocketConnect& sc, Header* phdr = 0)
	{
	char buf[MAXHDRSIZE];
	sc.ck_readline(buf, sizeof buf);
	if (0 == strcmp(buf, "EOF\r\n"))
		return Row::Eof;
	char* s = buf;
	Mmoffset recadr = int_to_mmoffset(ck_getnum('A', s));
	verify(recadr >= 0);
	int reclen = ck_getnum('R', s);
	if (phdr)
		*phdr = s_to_header(s);
	char* rec = new(noptrs) char[reclen + 1];
	sc.read(rec, reclen);
	rec[reclen] = 0; // so last field is nul terminated
	return Row(Record((void*) rec), recadr);
	}

Row DbmsQueryRemote::get(Dir dir)
	{
	WRITE("GET " << (dir == NEXT ? "+ " : "- ") << this);
	return getrec(sc);
	}

void DbmsQueryRemote::rewind()
	{
	WRITE("REWIND " << this);
	sc.ck_ok();
	}

static char* stripnl(char* s)
	{
	char* t = s + strlen(s) - 1;
	while (t >= s && (*t == '\r' || *t == '\n'))
		*t-- = 0;
	return s;
	}

char* DbmsQueryRemote::explain()
	{
	WRITE("EXPLAIN " << this);
	char buf[16000];
	sc.ck_readline(buf, sizeof buf);
	return dupstr(stripnl(buf));
	}

bool DbmsQueryRemote::output(const Record& rec)
	{
	int reclen = rec.cursize();
	CK(reclen);
	WRITEBUF("OUTPUT " << this << " R" << reclen);
	sc.write((char*) rec.dup().ptr(), reclen); // dup to squeeze?
	return sc.readbool();
	}

void DbmsQueryRemote::out(Ostream& os)
	{
	os << 'Q' << id;
	}

void DbmsQueryRemote::close()
	{
	WRITE("CLOSE " << this);
	sc.ck_ok();
	}

// DbmsCursorRemote ===================================================

class DbmsCursorRemote : public DbmsQueryRemote
	{
public:
	DbmsCursorRemote(CheckedSocketConnect& s, int i) : DbmsQueryRemote(s, i)
		{ }
	void set_transaction(int t);
	virtual void out(Ostream& os);
private:
	int tran = NO_TRAN;
	};

void DbmsCursorRemote::set_transaction(int t)
	{
	tran = t;
	}

void DbmsCursorRemote::out(Ostream& os)
	{
	if (isTran(tran))
		{
		os << "T" << tran << " ";
		tran = NO_TRAN;
		}
	os << 'C' << id;
	}

// DbmsRemote ========================================================

class DbmsRemote : public Dbms
	{
public:
	DbmsRemote(SocketConnect* s);
	~DbmsRemote();

	void abort(int tran) override;
	void admin(char* s) override;
	bool auth(const gcstring& data) override;
	Value check() override;
	bool commit(int tran, char** conflict) override;
	Value connections() override;
	DbmsQuery* cursor(char* s) override;
	int cursors() override;
	Value dump(char* filename) override;
	void erase(int tran, Mmoffset recadr) override;
	Value exec(Value ob) override;
	int final() override;
	Row get(Dir dir, char* query, bool one, Header& hdr, int tran = NO_TRAN) override;
	int kill(char* s) override;
	Lisp<gcstring> libget(char* name) override;
	Lisp<gcstring> libraries() override;
	int load(char* filename) override;
	void log(char* s) override;
	gcstring nonce() override;
	DbmsQuery* query(int tran, char* s) override;
	Value readCount(int tran) override;
	int request(int tran, char* s) override;
	Value run(char* s) override;
	Value sessionid(char* s) override;
	int64 size() override;
	int tempdest() override;
	gcstring token() override;
	Lisp<int> tranlist() override;
	int transaction(TranType type, char* session_id = "") override;
	Value timestamp() override;
	Mmoffset update(int tran, Mmoffset recadr, Record& rec) override;
	Value writeCount(int tran) override;
private:
	CheckedSocketConnect sc;
	OstreamStr os;
	};

static bool builtMatch(char* resp)
	{
	if (cmdlineoptions.ignore_version)
		return true;
	char* prefix = "Suneido Database Server (";
	if (!has_prefix(resp, prefix))
		return false;
	resp = resp + strlen(prefix);
	if (!strstr(resp, "Java"))
		return has_prefix(resp, build_date);
	else
		{
		// just compare date (not time)
		gcstring bd(build_date, 11); // 11 = MMM dd yyyy
		// squeeze out extra space for single digit dates on cSuneido
		bd = tr(bd, "  ", " ");
		return has_prefix(resp, bd.str());
		}
	}

DbmsRemote::DbmsRemote(SocketConnect* s) : sc(s)
	{
	char buf[80];
	sc.readline(buf, sizeof buf);
	if (! builtMatch(buf))
		except("connect failed\nexpected: Suneido Database Server ("
			<< build_date << ")\ngot: " << buf);
	sc.write("BINARY\r\n");
	sc.ck_ok();

	WRITE("SESSIONID ");
	sc.ck_readline(buf, sizeof buf);
	tls().fiber_id = dupstr(stripnl(buf));
	}

DbmsRemote::~DbmsRemote()
	{
	sc.close();
	}

int DbmsRemote::transaction(TranType type, char* session_id)
	{
	WRITE("TRANSACTION " << (type == READONLY ? "read" : "update"));
	return sc.readint('T');
	}

bool DbmsRemote::commit(int tran, char** conflict)
	{
	WRITE("COMMIT T" << tran);
	char buf[256];
	sc.ck_readline(buf, sizeof buf);
	if (0 == strcmp(buf, "OK\r\n"))
		return true;
	if (conflict)
		*conflict = dupstr(stripnl(buf));
	return false;
	}

void DbmsRemote::abort(int tran)
	{
	WRITE("ABORT T" << tran);
	sc.ck_ok();
	}

static char* nl_to_sp(char* s)
	{
	for (char* t = s; *t; ++t)
		if (*t == '\r' || *t == '\n')
			*t = ' ';
	return s;
	}

void DbmsRemote::admin(char* s)
	{
	WRITE("ADMIN " << nl_to_sp(s));
	sc.readbool();
	}

int DbmsRemote::request(int tran, char* s)
	{
	CK(strlen(s));
	WRITEBUF("REQUEST T" << tran << " Q" << strlen(s));
	sc.write(s);
	return sc.readint('R');
	}

DbmsQuery* DbmsRemote::cursor(char* s)
	{
	CK(strlen(s));
	WRITEBUF("CURSOR Q" << strlen(s));
	sc.write(s);
	return new DbmsCursorRemote(sc, sc.readint('C'));
	}

DbmsQuery* DbmsRemote::query(int tran, char* s)
	{
	CK(strlen(s));
	WRITEBUF("QUERY T" << tran << " Q" << strlen(s));
	LOG("c> " << s);
	sc.write(s);
	return new DbmsQueryRemote(sc, sc.readint('Q'));
	}

Lisp<gcstring> DbmsRemote::libget(char* name)
	{
	WRITE("LIBGET " << name);
	char buf[80];
	sc.ck_readline(buf, sizeof buf);
	int n;
	char* s = buf;
	Lisp<gcstring> srcs;
	while (ERR != (n = getnum('L', s)))
		{
		char buf2[80];
		sc.readline(buf2, sizeof buf2);
		srcs.push(stripnl(buf2)); // library

		gcstring src(n);
		sc.read(src.buf(), n);
		srcs.push(src); // text
		}
	return srcs.reverse();
	}

Lisp<gcstring> DbmsRemote::libraries()
	{
	sc.write("LIBRARIES\r\n");
	LOG("c> " << "LIBRARIES");
	char buf[1000];
	sc.ck_readline(buf, sizeof buf);
	if (buf[0] != '(' || buf[strlen(buf) - 3] != ')')
		except("LIBRARIES expected (...)\r\ngot: " << buf);
	Lisp<gcstring> libs;
	for (char* s = buf + 1; *s != '\r'; )
		{
		int n = strcspn(s, ",)");
		libs.push(gcstring(s, n));
		s += n + 1;
		}
	return libs.reverse();
	}

Lisp<int> DbmsRemote::tranlist()
	{
	sc.write("TRANLIST\r\n");
	LOG("c> " << "TRANLIST");
	char buf[1000];
	sc.ck_readline(buf, sizeof buf);
	if (buf[0] != '(' || buf[strlen(buf) - 3] != ')')
		except("TRANLIST expected (...)\r\ngot: " << buf);
	Lisp<int> trans;
	for (char* s = buf + 1; *s != '\r'; )
		{
		int n = strcspn(s, ",)");
		if (n)
			trans.push(atoi(s));
		s += n + 1;
		}
	return trans.reverse();
	}

Value DbmsRemote::timestamp()
	{
	sc.write("TIMESTAMP\r\n");
	LOG("c> " << "TIMESTAMP");
	char buf[80];
	sc.ck_readline(buf, sizeof buf);
	stripnl(buf);
	Value x = SuDate::literal(buf);
	if (! x)
		except("Timestamp: bad value from server: " << buf);
	return x;
	}

Value DbmsRemote::dump(char* filename)
	{
	WRITE("DUMP " << filename);
	return sc.readvalue();
	}

int DbmsRemote::load(char* filename)
	{
	WRITE("LOAD " << filename);
	return sc.readint('N');
	}

Value DbmsRemote::run(char* s)
	{
	WRITE("RUN " << nl_to_sp(s));
	return sc.readvalue();
	}

Value DbmsRemote::exec(Value ob)
	{
	int n = ob.packsize();
	CK(n);
	WRITEBUF("EXEC P" << n);
	char* buf = tmpalloc(n);
	ob.pack(buf);
	sc.write(buf, n);
	return sc.readvalue();
	}

int64 DbmsRemote::size()
	{
	sc.write("SIZE\r\n");
	LOG("c> " << "SIZE");
	return int_to_mmoffset(sc.readint('S'));
	}

Value DbmsRemote::connections()
	{
	sc.write("CONNECTIONS\r\n");
	LOG("c> " << "CONNECTIONS");
	return sc.readvalue();
	}

void DbmsRemote::erase(int tran, Mmoffset recadr)
	{
	WRITE("ERASE T" << tran << " A" << mmoffset_to_int(recadr));
	sc.ck_ok();
	}

Mmoffset DbmsRemote::update(int tran, Mmoffset recadr, Record& rec)
	{
	int reclen = rec.cursize();
	CK(reclen);
	WRITEBUF("UPDATE T" << tran << " A" << mmoffset_to_int(recadr) << " R" << reclen);
	sc.write((char*) rec.dup().ptr(), reclen); // dup only required for async ???
	return int_to_mmoffset(sc.readint('U'));
	}

Row DbmsRemote::get(Dir dir, char* query, bool one, Header& hdr, int tran)
	{
	CK(strlen(query));
	WRITEBUF("GET1 " << (dir == PREV ? "- " : (one ? "1 " : "+ "))
		<< " T" << tran << " Q" << strlen(query));
	LOG("c> " << query);
	sc.write(query);
	return getrec(sc, &hdr);
	}

int DbmsRemote::tempdest()
	{
	sc.write("TEMPDEST\r\n");
	LOG("c> " << "TEMPDEST");
	return sc.readint('D');
	}

int DbmsRemote::cursors()
	{
	sc.write("CURSORS\r\n");
	LOG("c> " << "CURSORS");
	return sc.readint('N');
	}

Value DbmsRemote::sessionid(char* s)
	{
	if (! *s)
		return new SuString(tls().fiber_id);
	WRITE("SESSIONID " << s);
	char buf[80];
	sc.ck_readline(buf, sizeof buf);
	SuString* ss = new SuString(stripnl(buf));
	tls().fiber_id = ss->str();
	return ss;
	}

int DbmsRemote::final()
	{
	sc.write("FINAL\r\n");
	LOG("c> " << "FINAL");
	return sc.readint('N');
	}

void DbmsRemote::log(char* s)
	{
	extern char* stripnl(char*);
	WRITE("LOG " << stripnl(s));
	sc.ck_ok();
	}

int DbmsRemote::kill(char* s)
	{
	WRITE("KILL " << s);
	return sc.readint('N');
	}

gcstring DbmsRemote::nonce()
	{
	sc.write("NONCE\r\n");
	gcstring buf(Auth::NONCE_SIZE);
	sc.read(buf.buf(), buf.size());
	return buf;
	}

gcstring DbmsRemote::token()
	{
	sc.write("TOKEN\r\n");
	gcstring buf(Auth::TOKEN_SIZE);
	sc.read(buf.buf(), buf.size());
	return buf;
	}

bool DbmsRemote::auth(const gcstring& data)
	{
	if (data.size() == 0)
		return false;
	WRITEBUF("AUTH D" << data.size());
	sc.write((char*) data.buf(), data.size());
	return sc.readbool();
	}

Value DbmsRemote::check()
	{
	sc.write("CHECK\r\n");
	return sc.readvalue();
	}

Value DbmsRemote::readCount(int tran)
	{
	WRITE("READCOUNT T" << tran);
	return sc.readint('C');
	}

Value DbmsRemote::writeCount(int tran)
	{
	WRITE("WRITECOUNT T" << tran);
	return sc.readint('C');
	}

// factory methods ==================================================

extern int su_port;

Dbms* dbms_remote_async(char* addr)
	{
	return new DbmsRemote(socketClientAsync(addr, su_port));
	}

static char* httpget(char* addr, int port)
	{
	try
		{
		SocketConnect* sc = socketClientSync(addr, port);
		OstreamStr oss;
		oss << "GET http://" << addr << "/:" << port << " HTTP/1.0\r\n\r\n";
		sc->write(oss.str());
		char buf[1024];
		int n = sc->read(buf, sizeof buf);
		sc->close();
		buf[n] = 0;
		return _strdup(buf);
		}
	catch (const Except&)
		{
		return "";
		}
	}

Dbms* dbms_remote(char* addr)
	{
	try
		{
		return new DbmsRemote(socketClientSync(addr, su_port));
		}
	catch (const Except&)
		{
		char* status = httpget(addr, su_port + 1);
		if (strstr(status, "Checking database ..."))
			fatal("Can't connect, server is checking the database, please try again later");
		else if (strstr(status, "Rebuilding database ..."))
			fatal("Can't connect, server is repairing the database, please try again later");
		throw;
		}
	}
