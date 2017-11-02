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
#include "fibers.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "dbms.h"
#include "win.h"
#include "errlog.h"
#include "builtinargs.h"
#include "exceptimp.h"
#include "readline.h"
#include "prim.h"


// SuSocketClient ===================================================

class SuSocketClient : public SuFinalize
	{
public:
	explicit SuSocketClient(SocketConnect* s);
	void out(Ostream& os) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;
	void close();
private:
	void finalize() override;
	void ckopen(const char* action);

	SocketConnect* sc;
	};

SuSocketClient::SuSocketClient(SocketConnect* s) : sc(s)
	{ }

void SuSocketClient::out(Ostream& os) const
	{
	os << "SocketClient /*builtin*/";
	}

Value SuSocketClient::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value Read("Read");
	static Value Readline("Readline");
	static Value Write("Write");
	static Value Writeline("Writeline");
	static Value Close("Close");

	BuiltinArgs args(nargs, nargnames, argnames, each);
	if (member == Read)
		{
		args.usage("socketClient.Read(size)");
		int n = args.getint("size");
		args.end();
		if (n == 0)
			return SuEmptyString;
		ckopen("Read");
		char* buf = salloc(n);
		int nr = sc->read(buf, n);
		if (nr <= 0)
			except("socket Read lost connection or timeout");
		return SuString::noalloc(buf, nr);
		}
	else if (member == Readline)
		{
		// NOTE: Readline should be consistent across file, socket, and runpiped
		NOARGS("socketClient.Readline()");
		ckopen("Readline");
		char buf[MAX_LINE + 1] = { 0 };
		sc->readline(buf, sizeof buf);
		if (! *buf)
			except("socket Readline lost connection or timeout");
		// assumes nul termination (so line may not contain nuls)
		for (int n = strlen(buf) - 1; n >= 0 && (buf[n] == '\r' || buf[n] == '\n'); --n)
			buf[n] = 0;
		return new SuString(buf);
		}
	else if (member == Write)
		{
		args.usage("socketClient.Write(string)");
		gcstring s = args.getValue("string").to_gcstr();
		args.end();
		ckopen("Write");
		sc->write(s);
		return Value();
		}
	else if (member == Writeline)
		{
		args.usage("socketClient.Writeline(string)");
		gcstring s = args.getValue("string").to_gcstr();
		args.end();
		ckopen("Writeline");
		sc->writebuf(s);
		sc->write("\r\n");
		return Value();
		}
	else if (member == Close)
		{
		NOARGS("socketClient.Close()");
		ckopen("Close");
		close();
		return Value();
		}
	else
		method_not_found("socketClient", member);
	}

void SuSocketClient::ckopen(const char* action)
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
	sc = nullptr;
	}

Value suSocketClient()
	{
	const int nargs = 5;
	auto ipaddr = ARG(0).str();
	int port = ARG(1).integer();
	int timeout = ARG(2).integer();
	int timeoutConnect = (int) (ARG(3).number()->to_double() * 1000); // milliseconds
	SuSocketClient* sc = new SuSocketClient(
		Fibers::inMain()
			? socketClientSync(ipaddr, port, timeout, timeoutConnect)
			: socketClientAsync(ipaddr, port, timeout, timeoutConnect));
	Value block = ARG(4);
	if (block == SuFalse)
		return sc;
	Closer<SuSocketClient*> closer(sc);
	KEEPSP
	PUSH(sc);
	return block.call(block, CALL, 1);
	}
PRIM(suSocketClient, "SocketClient(ipaddress, port, timeout=60, timeoutConnect=0, block=false)");

// SuSocketServer =====================================================
// the base for user defined Server classes

class SuSocketServer : public RootClass
	{
public:
	void out(Ostream& os) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;
	const char* type() const override
		{ return "BuiltinClass"; }
	};

void SuSocketServer::out(Ostream& os) const
	{
	os << "SocketServer /*builtin*/";
	}

static void _stdcall suserver(void* sc);

class SuServerInstance : public SuObject
	{
public:
	explicit SuServerInstance(SocketConnect* c) : sc(c)
		{ } // old way
	SuServerInstance() : sc(nullptr)
		{ } // master
	SuServerInstance(SuServerInstance* master, SocketConnect* s) : SuObject(*master), sc(s)
		{ } // dup
	void out(Ostream& os) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;
private:
	SocketConnect* sc;
	};

Value SuSocketServer::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	if (member == PARAMS)
		return new SuString("(name = .Name, port = .Port, exit = false, ...)");
	else if (member == CALL_CLASS)
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
		Value* a = (Value*) _alloca(sizeof (Value) * nargs);
		ushort* an = (ushort*) _alloca(sizeof (short) * (nargnames + 3));
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

		socketServer(name.str(), port.integer(), suserver, master, exit.toBool());
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
		Proc p; tls().proc = &p;

		// make an instance by duplicating the master
		sc = (SocketConnect*) arg;
		SuServerInstance* master = (SuServerInstance*) sc->getarg();
		SuValue* ob = new SuServerInstance(master, sc);

		static Value RUN("Run");
		ob->call(ob, RUN);
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

		delete tls().thedbms;
		tls().thedbms = nullptr;

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

void SuServerInstance::out(Ostream& os) const
	{
	os << "SocketServerConnection";
	}

Value SuServerInstance::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
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
			return SuEmptyString;
		char* buf = salloc(n);
		if (0 == sc->read(buf, n))
			except("socket server: lost connection");
		return SuString::noalloc(buf, n);
		}
	if (member == Readline)
		{
		NOARGS("socketServer.Readline()");
		char buf[2000];
		if (! sc->readline(buf, sizeof buf))
			except("socket server: lost connection");
		for (int n = strlen(buf) - 1; n >= 0 && (buf[n] == '\r' || buf[n] == '\n'); --n)
			buf[n] = 0;
		return new SuString(buf);
		}
	else if (member == Write)
		{
		if (nargs != 1)
			except("usage: socketServer.Write(string)");
		sc->write(TOP().gcstr());
		return Value();
		}
	else if (member == Writeline)
		{
		if (nargs != 1)
			except("usage: socketServer.Writeline(string)");
		sc->write(TOP().gcstr());
		sc->write("\r\n");
		return Value();
		}
	else if (member == RemoteUser)
		{
		NOARGS("socketServer.RemoteUser()");
		return new SuString(sc->getadr());
		}
	else
		return SuObject::call(self, member, nargs, nargnames, argnames, each);
	}
