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

#include "connection.h"
#include "dbms.h"
#include "lisp.h"
#include "row.h"
#include "clientserver.h"
#include "sockets.h"
#include "sustring.h"
#include "fibers.h"
#include <cctype>
#include <vector>
#include "cmdlineoptions.h"
#include "build.h"
#include "tr.h"

// the client side of the *binary* client-server protocol

#define DbmsRemote DbmsRemote2
#define DbmsQueryRemote DbmsQueryRemote2
#define DbmsCursorRemote DbmsCursorRemote2

enum class CorQ : char { CURSOR = 'c', QUERY = 'q' };

class DbmsRemote : public Dbms
	{
	friend class DbmsQueryRemote;
public:
	explicit DbmsRemote(SocketConnect* s);
	~DbmsRemote();

	void abort(int tn) override;
	void admin(const char* s) override;
	bool auth(const gcstring& data) override;
	Value check() override;
	bool commit(int tn, const char** conflict) override;
	Value connections() override;
	DbmsQuery* cursor(const char* query) override;
	int cursors() override;
	Value dump(const char* filename) override;
	void erase(int tn, Mmoffset recadr) override;
	Value exec(Value ob) override;
	int final() override;
	Row get(Dir dir, const char* query, bool one, Header& hdr, int tn = NO_TRAN) override;
	int kill(const char* sessionid) override;
	Lisp<gcstring> libget(const char* name) override;
	Lisp<gcstring> libraries() override;
	int load(const char* filename) override;
	void log(const char* s) override;
	gcstring nonce() override;
	DbmsQuery* query(int tn, const char* query) override;
	int readCount(int tn) override;
	int request(int tn, const char* s) override;
	Value run(const char* s) override;
	Value sessionid(const char* s) override;
	int64 size() override;
	int tempdest() override;
	Value timestamp() override;
	gcstring token() override;
	Lisp<int> tranlist() override;
	int transaction(TranType type, const char* session_id = "") override;
	Mmoffset update(int tn, Mmoffset recadr, Record& rec) override;
	int writeCount(int tn) override;
private:
	static bool checkHello(const gcstring& hello);
	void close(int qn, CorQ cq);
	const char* explain(int qn, CorQ cq);
	Row get(int tn, int qn, Dir dir);
	Header header(int qn, CorQ cq);
	Lisp<Lisp<gcstring>> keys(int qn, CorQ cq);
	Lisp<gcstring> order(int qn, CorQ cq);
	bool output(int qn, const Record& rec);
	void rewind(int qn, CorQ cq);

	void send(Command cmd);
	void send(Command cmd, bool b);
	void send(Command cmd, int n);
	void send(Command cmd, int n, int m);
	void send(Command cmd, int tn, int qn, Dir dir);
	void send(Command cmd, Dir dir, bool one, int tn, const char* query);
	void send(Command cmd, int qn, const Record& rec);
	void send(Command cmd, int tn, int recadr, const Record& rec);
	void send(Command cmd, int n, CorQ cq);
	void send(Command cmd, int n, const gcstring& s);
	void send(Command cmd, const char* s);
	void send(Command cmd, const gcstring& s);
	void send(Command cmd, Value val);
	Serializer& putCmd(Command cmd);
	void doRequest();
	void getResponse();
	Header getHeader();
	Row getRow(Header* phdr = nullptr);

	ClientConnection io;
	};

class DbmsQueryRemote : public DbmsQuery
	{
public:
	DbmsQueryRemote(DbmsRemote& d, int q) : dr(d), qn(q)
		{ }
	void set_transaction(int tn) override
		{ }
	void close() override
		{ return dr.close(qn, c_or_q()); }
	const char* explain() override
		{ return dr.explain(qn, c_or_q()); }
	Row get(Dir dir) override
		{ return dr.get(getTran(), qn, dir); }
	Header header() override
		{
		if (nil(header_))
			header_ = dr.header(qn, c_or_q());
		return header_;
		}
	Lisp<Lisp<gcstring>> keys() override
		{
		if (nil(keys_))
			keys_ = dr.keys(qn, c_or_q());
		return keys_;
		}
	Lisp<gcstring> order() override
		{ return dr.order(qn, c_or_q()); }
	bool output(const Record& rec) override
		{ return dr.output(qn, rec); }
	void rewind() override
		{ return dr.rewind(qn, c_or_q()); }
protected:
	virtual CorQ c_or_q()
		{
		return CorQ::QUERY;
		}
	virtual int getTran()
		{ return NO_TRAN; }

	DbmsRemote& dr;
	const int qn;
	Header header_; // cache
	Lisp<Lisp<gcstring>> keys_; // cache
	};

class DbmsCursorRemote : public DbmsQueryRemote
	{
public:
	DbmsCursorRemote(DbmsRemote& d, int c) : DbmsQueryRemote(d, c)
		{ }
	void set_transaction(int t) override
		{ tn = t; }
protected:
	CorQ c_or_q() override
		{ return CorQ::CURSOR; }
	int getTran() override
		{
		verify(isTran(tn));
		int t = tn;
		tn = NO_TRAN; // clear after use
		return t;
		}
private:
	int tn = NO_TRAN;
	};

// DbmsRemote -----------------------------------------------------------------

DbmsRemote::DbmsRemote(SocketConnect* sc) : io(sc)
	{
	gcstring hello = sc->read(HELLO_SIZE);
	if (!checkHello(hello))
		except("connect failed\n" <<
			"client: Suneido " << build << "\n" <<
			"server: " << hello);
	}

bool DbmsRemote::checkHello(const gcstring& hello)
	{
	if (cmdlineoptions.ignore_version)
		return true;
	const char* prefix = "Suneido ";
	if (!hello.has_prefix(prefix) || hello.has_prefix("Suneido Database Server"))
		return false;
	gcstring rest = hello.substr(strlen(prefix));
	if (!rest.has("Java"))
		return rest.has_prefix(build); // exact match
	else
		{
		// just compare date (not time)
		gcstring bd(build, 11); // 11 = MMM dd yyyy
		bd = tr(bd, "  ", " "); // squeeze out extra space for single digit dates
		return rest.has_prefix(bd.str());
		}
	}

DbmsRemote::~DbmsRemote()
	{
	io.close();
	}

void DbmsRemote::abort(int tn)
	{
	send(Command::ABORT, tn);
	}

void DbmsRemote::admin(const char* s)
	{
	send(Command::ADMIN, s);
	}

bool DbmsRemote::auth(const gcstring& data)
	{
	send(Command::AUTH, data);
	return io.getBool();
	}

Value DbmsRemote::check()
	{
	send(Command::CHECK);
	return new SuString(io.getStr());
	}

void DbmsRemote::close(int qn, CorQ cq) // DbmsQuery
	{
	send(Command::CLOSE, qn, cq);
	}

bool DbmsRemote::commit(int tn, const char** conflict)
	{
	send(Command::COMMIT, tn);
	if (io.getBool())
		return true;
	*conflict = io.getStr().str();
	return false;
	}

Value DbmsRemote::connections()
	{
	send(Command::CONNECTIONS);
	return io.getValue();
	}

DbmsQuery* DbmsRemote::cursor(const char* query)
	{
	send(Command::CURSOR, query);
	auto cn = io.getInt();
	return new DbmsCursorRemote(*this, cn);
	}

int DbmsRemote::cursors()
	{
	send(Command::CURSORS);
	return io.getInt();
	}

Value DbmsRemote::dump(const char* filename)
	{
	send(Command::DUMP, filename);
	return new SuString(io.getStr());
	}

void DbmsRemote::erase(int tn, Mmoffset recadr)
	{
	send(Command::ERASE, tn, recadr);
	}

Value DbmsRemote::exec(Value ob)
	{
	send(Command::EXEC, ob);
	return io.getBool() ? io.getValue() : Value();
	}

const char* DbmsRemote::explain(int qn, CorQ cq) // DbmsQuery
	{
	send(Command::EXPLAIN, qn, cq);
	return io.getStr().str();
	}

int DbmsRemote::final()
	{
	send(Command::FINAL);
	return io.getInt();
	}

Row DbmsRemote::get(Dir dir, const char* query, bool one, Header& hdr, int tn)
	{
	send(Command::GET1, dir, one, tn, query);
	return getRow(&hdr);
	}

Row DbmsRemote::get(int tn, int qn, Dir dir) // DbmsQuery
	{
	send(Command::GET, tn, qn, dir);
	return getRow();
	}

Row DbmsRemote::getRow(Header* phdr)
	{
	if (!io.getBool())
		return Row::Eof;
	int64_t recadr = static_cast<unsigned int>(io.getInt());
	if (phdr)
		*phdr = getHeader();
	gcstring r = io.getBuf();
	return Row(Record(r.ptr()), recadr);
	}

int DbmsRemote::kill(const char* sessionid)
	{
	send(Command::KILL, sessionid);
	return io.getInt();
	}

Header DbmsRemote::header(int qn, CorQ cq) // DbmsQuery
	{
	send(Command::HEADER, qn, cq);
	return getHeader();
	}

Header DbmsRemote::getHeader()
	{
	int n = io.getInt();
	Lisp<gcstring> cols;
	Lisp<gcstring> fields;
	for (int i = 0; i < n; ++i)
		{
		gcstring s = io.getStr();
		if (isupper(s[0]))
			s = s.uncapitalize();
		else
			fields.push(s);
		if (s != "-")
			cols.push(s);
		}
	cols.reverse();
	fields.reverse();
	return Header(lisp(fields), cols);
	}

Lisp<Lisp<gcstring>> DbmsRemote::keys(int qn, CorQ cq) // DbmsQuery
	{
	send(Command::KEYS, qn, cq);
	int n = io.getInt();
	Lisp<Lisp<gcstring>> list;
	for (int i = 0; i < n; ++i)
		list.push(io.getStrings());
	return list.reverse();
	}

Lisp<gcstring> DbmsRemote::libget(const char* name)
	{
	send(Command::LIBGET, name);
	int n = io.getInt();
	std::vector<gcstring> libs(n);
	std::vector<int> sizes(n);
	for (int i = 0; i < n; ++i)
		{
		libs[i] = io.getStr();
		sizes[i] = io.getInt();
		}
	Lisp<gcstring> srcs;
	for (int i = 0; i < n; ++i)
		{
		srcs.push(libs[i]);
		gcstring src = io.read(sizes[i]);
		srcs.push(src); // text
		}
	return srcs.reverse();
	}

Lisp<gcstring> DbmsRemote::libraries()
	{
	send(Command::LIBRARIES);
	return io.getStrings();
	}

int DbmsRemote::load(const char* filename)
	{
	send(Command::LOAD, filename);
	return io.getInt();
	}

void DbmsRemote::log(const char* s)
	{
	send(Command::LOG, s);
	}

gcstring DbmsRemote::nonce()
	{
	send(Command::NONCE);
	return io.getStr();
	}

Lisp<gcstring> DbmsRemote::order(int qn, CorQ cq) // DbmsQuery
	{
	send(Command::ORDER, qn, cq);
	return io.getStrings();
	}

bool DbmsRemote::output(int qn, const Record& rec) // DbmsQuery
	{
	send(Command::OUTPUT, qn, rec);
	return true;
	}

DbmsQuery* DbmsRemote::query(int tn, const char* query)
	{
	send(Command::QUERY, tn, query);
	int qn = io.getInt();
	return new DbmsQueryRemote(*this, qn);
	}

int DbmsRemote::readCount(int tn)
	{
	send(Command::READCOUNT);
	return io.getInt();
	}

int DbmsRemote::request(int tn, const char* s)
	{
	send(Command::REQUEST, tn, s);
	return io.getInt();
	}

void DbmsRemote::rewind(int qn, CorQ cq) // DbmsQuery
	{
	send(Command::REWIND, qn, cq);
	}

Value DbmsRemote::run(const char* s)
	{
	send(Command::RUN, s);
	return io.getBool() ? io.getValue() : Value();
	}

Value DbmsRemote::sessionid(const char* sessionid)
	{
	if (*sessionid || !*tls().fiber_id)
		{
		send(Command::SESSIONID, sessionid);
		tls().fiber_id = io.getStr().str();
		}
	return new SuString(tls().fiber_id);
	}

int64 DbmsRemote::size()
	{
	send(Command::SIZE);
	return io.getInt();
	}

int DbmsRemote::tempdest()
	{
	return 0;
	}

Value DbmsRemote::timestamp()
	{
	send(Command::TIMESTAMP);
	return io.getValue();
	}

gcstring DbmsRemote::token()
	{
	send(Command::TOKEN);
	return io.getStr();
	}

Lisp<int> DbmsRemote::tranlist()
	{
	send(Command::TRANSACTIONS);
	return io.getInts();
	}

int DbmsRemote::transaction(TranType type, const char* session_id)
	{
	send(Command::TRANSACTION, type == READWRITE);
	return io.getInt();
	}

Mmoffset DbmsRemote::update(int tn, Mmoffset recadr, Record& rec)
	{
	send(Command::UPDATE, tn, recadr, rec);
	return io.getInt();
	}

int DbmsRemote::writeCount(int tn)
	{
	send(Command::WRITECOUNT);
	return io.getInt();
	}

//-----------------------------------------------------------------------------

void DbmsRemote::send(Command cmd)
	{
	putCmd(cmd);
	doRequest();
	}

void DbmsRemote::send(Command cmd, bool b)
	{
	putCmd(cmd).putBool(b);
	doRequest();
	}

void DbmsRemote::send(Command cmd, int n)
	{
	putCmd(cmd).putInt(n);
	doRequest();
	}

void DbmsRemote::send(Command cmd, int n, int m)
	{
	putCmd(cmd).putInt(n).putInt(m);
	doRequest();
	}

void DbmsRemote::send(Command cmd, int tn, int qn, Dir dir)
	{
	putCmd(cmd).put(dir == NEXT ? '+' : '-').putInt(tn).putInt(qn);
	doRequest();
	}

void DbmsRemote::send(Command cmd, Dir dir, bool one, int tn, const char * query)
	{
	putCmd(cmd).put(one ? '1' : (dir == NEXT) ? '+' : '-')
		.putInt(tn).putStr(query);
	doRequest();
	}

void DbmsRemote::send(Command cmd, int qn, const Record & rec)
	{
	putCmd(cmd).putInt(qn).putInt(rec.cursize());
	io.write(static_cast<char*>(rec.dup().ptr()), rec.cursize());
	doRequest();
	}

void DbmsRemote::send(Command cmd, int tn, int recadr, const Record & rec)
	{
	putCmd(cmd).putInt(tn).putInt(recadr).putInt(rec.cursize());
	io.write(static_cast<char*>(rec.dup().ptr()), rec.cursize());
	doRequest();
	}

void DbmsRemote::send(Command cmd, int n, CorQ cq)
	{
	putCmd(cmd).putInt(n).put(char(cq));
	doRequest();
	}

void DbmsRemote::send(Command cmd, int n, const gcstring& s)
	{
	putCmd(cmd).putInt(n).putStr(s);
	doRequest();
	}

void DbmsRemote::send(Command cmd, const char* s)
	{
	putCmd(cmd).putStr(s);
	doRequest();
	}

void DbmsRemote::send(Command cmd, const gcstring& s)
	{
	putCmd(cmd).putStr(s);
	doRequest();
	}

void DbmsRemote::send(Command cmd, Value val)
	{
	putCmd(cmd).putValue(val);
	doRequest();
	}

Serializer& DbmsRemote::putCmd(Command cmd)
	{
	io.clear();
	return io.putCmd(char(cmd));
	}

void DbmsRemote::doRequest()
	{
	io.write();
	getResponse();
	}

void DbmsRemote::getResponse()
	{
	if (!io.getBool())
		{
		auto err = io.getStr();
		except(err << " (from server)");
		}
	}

// factory methods ------------------------------------------------------------

#include "ostreamstr.h"
#include "exceptimp.h"
#include "fatal.h"

extern int su_port;

Dbms* dbms_remote2_async(char* addr)
	{
	return new DbmsRemote2(socketClientAsync(addr, su_port));
	}

static const char* httpget(const char* addr, int port)
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
		return dupstr(buf);
		}
	catch (const Except&)
		{
		return "";
		}
	}

Dbms* dbms_remote2(char* addr)
	{
	try
		{
		return new DbmsRemote2(socketClientSync(addr, su_port));
		}
	catch (const Except&)
		{
		const char* status = httpget(addr, su_port + 1);
		if (strstr(status, "Checking database ..."))
			fatal("Can't connect, server is checking the database, please try again later");
		else if (strstr(status, "Rebuilding database ..."))
			fatal("Can't connect, server is repairing the database, please try again later");
		throw;
		}
	}
