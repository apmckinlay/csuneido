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

#define DO(fn) try { fn; } catch (const Except* e) { fatal("lost connection:", e->str()); exit(0); }

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
			except(buf + 4);
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
		gcstring result(n); // would alloca work?
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
	return strdup(stripnl(buf));
	}

bool DbmsQueryRemote::output(const Record& rec)
	{
	int reclen = rec.cursize();
	WRITEBUF("OUTPUT " << this << " R" << reclen);
	sc.write((char*) rec.dup().ptr(), reclen);
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
	DbmsCursorRemote(CheckedSocketConnect& s, int i) : DbmsQueryRemote(s, i), tran(-1)
		{ }
	void set_transaction(int t);
	virtual void out(Ostream& os);
private:
	int tran;
	};

void DbmsCursorRemote::set_transaction(int t)
	{
	tran = t;
	}

void DbmsCursorRemote::out(Ostream& os)
	{
	if (tran >= 0)
		{
		os << "T" << tran << " ";
		tran = -1;
		}
	os << 'C' << id;
	}

// DbmsRemote ========================================================

class DbmsRemote : public Dbms
	{
public:
	DbmsRemote(SocketConnect* s);
	~DbmsRemote();
	int transaction(TranType type, char* session_id = "");
	bool commit(int tran, char** conflict);
	void abort(int tran);

	bool admin(char* s);
	int request(int tran, char* s);
	DbmsQuery* cursor(char* s);
	DbmsQuery* query(int tran, char* s);
	Lisp<gcstring> libget(char* name);
	Lisp<gcstring> libraries();
	Lisp<int> tranlist();
	Value timestamp();
	void dump(char* filename);
	void copy(char* filename);
	Value run(char* s);
	int64 size();
	Value connections();
	void erase(int tran, Mmoffset recadr);
	Mmoffset update(int tran, Mmoffset recadr, Record& rec);
	bool record_ok(int tran, Mmoffset recadr);
	Row get(Dir dir, char* query, bool one, Header& hdr, int tran = -1);
	int tempdest();
	int cursors();
	Value sessionid(char* s);
	bool refresh(int tran);
	int final();
	void log(char* s);
	int kill(char* s);
private:
	CheckedSocketConnect sc;
	OstreamStr os;
	};

char* session_id = "";

DbmsRemote::DbmsRemote(SocketConnect* s) : sc(s)
	{
	char buf[80];
	sc.readline(buf, sizeof buf);
	os << "Suneido Database Server (" << build_date << ")\r\n";
	if (! cmdlineoptions.ignore_version && 
		0 != strcmp(buf, os.str()))
		except("connect failed\nexpected: " << os.str() << "\ngot: " << buf);
	sc.write("BINARY\r\n");
	sc.ck_ok();

	WRITE("SESSIONID ");
	sc.ck_readline(buf, sizeof buf);
	session_id = strdup(stripnl(buf));
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
		*conflict = strdup(stripnl(buf));
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

bool DbmsRemote::admin(char* s)
	{
	WRITE("ADMIN " << nl_to_sp(s));
	return sc.readbool();
	}

int DbmsRemote::request(int tran, char* s)
	{
	WRITEBUF("REQUEST T" << tran << " Q" << strlen(s));
	sc.write(s);
	return sc.readint('R');
	}

DbmsQuery* DbmsRemote::cursor(char* s)
	{
	WRITEBUF("CURSOR Q" << strlen(s));
	sc.write(s);
	return new DbmsCursorRemote(sc, sc.readint('C'));
	}

DbmsQuery* DbmsRemote::query(int tran, char* s)
	{
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
	char buf[80];
	sc.ck_readline(buf, sizeof buf);
	stripnl(buf);
	Value x = SuDate::literal(buf);
	if (! x)
		except("Timestamp: bad value from server: " << buf);
	return x;
	}

void DbmsRemote::dump(char* filename)
	{
	WRITE("DUMP " << filename);
	sc.ck_ok();
	}

void DbmsRemote::copy(char* filename)
	{
	WRITE("COPY " << filename);
	sc.ck_ok();
	}

Value DbmsRemote::run(char* s)
	{
	WRITE("RUN " << nl_to_sp(s));
	return sc.readvalue();
	}

int64 DbmsRemote::size()
	{
	sc.write("SIZE\r\n");
	return int_to_mmoffset(sc.readint('S'));
	}

Value DbmsRemote::connections()
	{
	sc.write("CONNECTIONS\r\n");
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
	WRITEBUF("UPDATE T" << tran << " A" << mmoffset_to_int(recadr) << " R" << reclen);
	sc.write((char*) rec.dup().ptr(), reclen);
	return int_to_mmoffset(sc.readint('U'));
	}

bool DbmsRemote::record_ok(int tran, Mmoffset recadr)
	{
	WRITE("RECORDOK T" << tran << " A" << mmoffset_to_int(recadr));
	return sc.readbool();
	}

Row DbmsRemote::get(Dir dir, char* query, bool one, Header& hdr, int tran)
	{
	WRITEBUF("GET1 " << (dir == PREV ? "- " : (one ? "1 " : "+ ")) 
		<< " T" << tran << " Q" << strlen(query));
	LOG("c> " << query);
	sc.write(query);
	return getrec(sc, &hdr);
	}

int DbmsRemote::tempdest()
	{
	sc.write("TEMPDEST\r\n");
	return sc.readint('D');
	}

int DbmsRemote::cursors()
	{
	sc.write("CURSORS\r\n");
	return sc.readint('N');
	}

Value DbmsRemote::sessionid(char* s)
	{
	WRITE("SESSIONID " << s);
	char buf[80];
	sc.ck_readline(buf, sizeof buf);
	SuString* ss = new SuString(stripnl(buf));
	session_id = ss->str();
	return ss;
	}

bool DbmsRemote::refresh(int tran)
	{
	WRITE("REFRESH T" << tran << "\r\n");
	return sc.readbool();
	}

int DbmsRemote::final()
	{
	sc.write("FINAL\r\n");
	return sc.readint('N');
	}

void DbmsRemote::log(char* s)
	{
	extern char* stripnl(char* s);
	WRITE("LOG " << stripnl(s));
	sc.ck_ok();
	}

int DbmsRemote::kill(char* s)
	{
	WRITE("KILL " << s << "\r\n");
	return sc.readint('N');
	}

// factory methods ==================================================

extern int su_port;

Dbms* dbms_remote(char* addr)
	{
	return new DbmsRemote(socketClientSynch(addr, su_port));
	}

Dbms* dbms_remote_asynch(char* addr)
	{
	return new DbmsRemote(socketClientAsynch(addr, su_port));
	}
