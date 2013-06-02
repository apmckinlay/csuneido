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

#include "susockets.h"
#include "sockets.h"
#include "sufinalize.h"
#include "suobject.h"
#include "symbols.h"
#include "interp.h"
#include "sustring.h"
#include "suclass.h"
#include "suboolean.h"
#include "fibers.h"
#include "dbms.h"
#include "win.h"
#include "errlog.h"
#include "builtinargs.h"
#include "exceptimp.h"

// SuSocketClient ===================================================

class SuSocketClient : public SuFinalize
	{
public:
	SuSocketClient(SocketConnect* s);
	void out(Ostream& os);
	Value call(Value self, Value member, short nargs, short nargnames,
		ushort* argnames, int each);
	void close();
private:
	virtual void finalize();
	void ckopen(char* action);

	SocketConnect* sc;
	};

SuSocketClient::SuSocketClient(SocketConnect* s) : sc(s)
	{ }

void SuSocketClient::out(Ostream& os)
	{
	os << "SocketClient /*builtin*/";
	}

Value SuSocketClient::call(Value self, Value member, short nargs,
	short nargnames, ushort* argnames, int each)
	{
	static Value Read("Read");
	static Value Readline("Readline");
	static Value Write("Write");
	static Value Writeline("Writeline");
	static Value Close("Close");

	BuiltinArgs args(nargs, nargnames, argnames, each);
	if (member == Read)
		{
		args.usage("usage: socketClient.Read(size)");
		int n = args.getint("size");
		args.end();
		if (n == 0)
			return "";
		ckopen("Read");
		SuString* s = new SuString(n);
		int nr = sc->read(s->buf(), n);
		if (nr <= 0)
			except("socket client: lost connection or timeout in Read(" << n << ")");
		return s->substr(0, nr);
		}
	else if (member == Readline)
		{
		if (nargs != 0)
			except("usage: socketClient.Readline()");
		ckopen("Readline");
		char buf[2000];
		sc->readline(buf, 2000);
		if (! *buf)
			except("socket client: lost connection or timeout in Readline");
		for (int n = strlen(buf) - 1; (n >= 0 && buf[n] == '\r') || buf[n] == '\n'; --n)
			buf[n] = 0;
		return new SuString(buf);
		}
	else if (member == Write)
		{
		args.usage("usage: socketClient.Write(string)");
		gcstring s = args.getgcstr("string");
		args.end();
		ckopen("Write");
		sc->write(s.buf(), s.size());
		return Value();
		}
	else if (member == Writeline)
		{
		args.usage("usage: socketClient.Writeline(string)");
		gcstring s = args.getgcstr("string");
		args.end();
		ckopen("Writeline");
		sc->writebuf(s.buf(), s.size());
		sc->write("\r\n", 2);
		return Value();
		}
	else if (member == Close)
		{
		if (nargs != 0)
			except("usage: socketClient.Close()");
		ckopen("Close");
		close();
		return Value();
		}
	else
		method_not_found("socketClient", member);
	}

void SuSocketClient::ckopen(char* action)
	{
	if (! sc)
		except("SocketClient: can't " << action << " a closed socket");
	}

void SuSocketClient::close()
	{
	removefinal();
	finalize();
	}

void SuSocketClient::finalize()
	{
	if (sc)
		sc->close();
	sc = 0;
	}

#include "prim.h"

Value suSocketClient()
	{
	const int nargs = 5;
	char* ipaddr = ARG(0).str();
	int port = ARG(1).integer();
	int timeout = ARG(2).integer();
	int timeoutConnect = (int) (ARG(3).number()->to_double() * 1000);
	SuSocketClient* sc = new SuSocketClient(
		Fibers::current() == Fibers::main()
			? socketClientSynch(ipaddr, port, timeout, timeoutConnect)
			: socketClientAsynch(ipaddr, port));
	Value block = ARG(4);
	if (block == SuFalse)
		return sc;
	Closer<SuSocketClient*> closer(sc);
	KEEPSP
	PUSH(sc);
	return block.call(block, CALL, 1, 0, 0, -1);
	}
PRIM(suSocketClient, "SocketClient(ipaddress, port, timeout=60, timeoutConnect=0, block=false)");

// SuSocketServer =====================================================
// the base for user defined Server classes

class SuSocketServer : public RootClass
	{
public:
	void out(Ostream& os);
	Value call(Value self, Value member, short nargs, short nargnames,
		ushort* argnames, int each);
	};

void SuSocketServer::out(Ostream& os)
	{
	os << "SocketServer /*builtin*/";
	}

static void _stdcall suserver(void* sc);

class SuServerInstance : public SuObject
	{
public:
	SuServerInstance(SocketConnect* c) : sc(c)
		{ } // old way
	SuServerInstance() : sc(0)
		{ } // master
	SuServerInstance(SuServerInstance* master, SocketConnect* s) : SuObject(*master), sc(s)
		{ } // dup
	void out(Ostream& os);
	Value call(Value self, Value member, short nargs, short nargnames,
		ushort* argnames, int each);
	SocketConnect* sc;
	};

Value SuSocketServer::call(Value self, Value member, short nargs,
	short nargnames, ushort* argnames, int each)
	{
	if (member == CALL_CLASS)
		{
		static Value Name("Name");
		static Value Port("Port");
		static int NAME = ::symnum("name");
		static int PORT = ::symnum("port");
		static int EXIT = ::symnum("exit");
	
		SuObject* selfob = self.object();
		BuiltinArgs args(nargs, nargnames, argnames, each);
		args.usage("SocketServer(name = .Name, port = .Port, exit = false, ...)");
		Value name = args.getValue("name", Value());
		Value port = args.getValue("port", Value());
		Value exit = args.getValue("exit", Value());
		
		// convert arguments, make name, port, and exit named
		int na = 0;
		Value* a = (Value*) alloca(sizeof (Value) * nargs);
		ushort* an = (ushort*) alloca(sizeof (short) * (nargnames + 3));
		short nan = 0;
		while (Value arg = args.getNext())
			{
			a[na++] = arg;
			if (ushort n = args.curName())
				an[nan++] = n;
			}
		KEEPSP
		for (int i = 0; i < na; ++i)
			PUSH(a[i]);
		if (name)
			{ PUSH(name); an[nan++] = NAME; ++na; }
		else
			name = selfob->getdata(Name);
		if (port)
			{ PUSH(port); an[nan++] = PORT; ++na; }
		else
			port = selfob->getdata(Port);
		if (exit)
			{ PUSH(exit); an[nan++] = EXIT; ++na; }
		else
			exit = SuFalse;
		
		// construct a "master" instance, which will be duplicated for each connection
		SuServerInstance* master = new SuServerInstance();
		master->myclass = self;
		master->call(master, NEW, na, nan, an, -1); 

		socketServer(name.str(), port.integer(), suserver, master, exit == SuTrue);
		}
	return Value();
	}

Value suSocketServer()
	{
	static SuSocketServer* suss = new SuSocketServer();
	return suss;
	}

// SuServerInstance & Server ========================================

static void _stdcall suserver(void* arg)
	{
	SocketConnect* sc = 0;
	try
		{
		Proc p; tss_proc() = &p;

		// make an instance by duplicating the master
		sc = (SocketConnect*) arg;
		SuServerInstance* master = (SuServerInstance*) sc->getarg();
		SuServerInstance* ob = new SuServerInstance(master, sc);

		static Value RUN("Run");
		ob->call(ob, RUN, 0, 0, 0, -1);
		}
	catch (const Except& e)
		{
		errlog("exception in SocketServer: " , e.str());
		}
	catch (const std::exception& e)
		{
		errlog("exception in SocketServer: ", e.what());
		}
	catch (...)
		{
		errlog("unknown exception in SocketServer");
		}
	try
		{
		if (sc)
			sc->close();

		extern Dbms*& tss_thedbms();
		delete tss_thedbms();
		tss_thedbms() = 0;

		Fibers::end();
		}
	catch (const Except& e)
		{
		errlog("exception closing SocketServer connection: " , e.str());
		}
	catch (const std::exception& e)
		{
		errlog("exception closing SocketServer connection: ", e.what());
		}
	catch (...)
		{
		errlog("unknown exception closing SocketServer connection");
		}
	}

void SuServerInstance::out(Ostream& os)
	{
	os << "SocketServerConnection";
	}

Value SuServerInstance::call(Value self, Value member, short nargs,
	short nargnames, ushort* argnames, int each)
	{
	static Value Read("Read");
	static Value Readline("Readline");
	static Value Write("Write");
	static Value Writeline("Writeline");
	static Value RemoteUser("RemoteUser");

	if (member == Read)
		{
		if (nargs != 1)
			except("usage: socketServer.Read(size)");
		int n = ARG(0).integer();
		if (n == 0)
			return "";
		SuString* s = new SuString(n);
		if (0 == sc->read(s->buf(), n))
			except("socket server: lost connection");
		return s;
		}
	if (member == Readline)
		{
		if (nargs != 0)
			except("usage: socketServer.Readline()");
		char buf[2000];
		if (! sc->readline(buf, sizeof buf))
			except("socket server: lost connection");
		for (int n = strlen(buf) - 1; (n >= 0 && buf[n] == '\r') || buf[n] == '\n'; --n)
			buf[n] = 0;
		return new SuString(buf);
		}
	else if (member == Write)
		{
		if (nargs != 1)
			except("usage: socketServer.Write(string)");
		gcstring s = TOP().gcstr();
		sc->write(s.buf(), s.size());
		return Value();
		}
	else if (member == Writeline)
		{
		if (nargs != 1)
			except("usage: socketServer.Writeline(string)");
		gcstring s = TOP().gcstr();
		sc->write(s.buf(), s.size());
		sc->write("\r\n", 2);
		return Value();
		}
	else if (member == RemoteUser)
		{
		if (nargs != 0)
			except("usage: socketServer.RemoteUser()");
		return new SuString(sc->getadr());
		}
	else
		return SuObject::call(self, member, nargs, nargnames, argnames, each);
	}

#include "prim.h"

Value su_SocketConnectionCount()
	{
	return socketConnectionCount();
	}
PRIM(su_SocketConnectionCount, "SocketConnectionCount()");
