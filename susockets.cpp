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
		ckopen("Read");
		SuString* s = new SuString(n);
		int nr = sc->read(s->buf(), n);
		if (nr == 0)
			except("socket client: lost connection or timeout");
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
			except("socket client: lost connection or timeout");
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
		unknown_method("socketClient", member);
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
	const int nargs = 4;
	char* ipaddr = ARG(0).str();
	int port = ARG(1).integer();
	int timeout = ARG(2).integer();
	SuSocketClient* sc = new SuSocketClient(
		Fibers::current() == Fibers::main()
			? socketClientSynch(ipaddr, port, timeout)
			: socketClientAsynch(ipaddr, port));
	Value block = ARG(3);
	if (block == SuFalse)
		return sc;
	Closer<SuSocketClient*> closer(sc);
	KEEPSP
	PUSH(sc);
	return block.call(block, CALL, 1, 0, 0, -1);
	}
PRIM(suSocketClient, "SocketClient(ipaddress, port, timeout=60, block=false)");

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

Value SuSocketServer::call(Value self, Value member, short nargs, 
	short nargnames, ushort* argnames, int each)
	{
	if (member == CALL_CLASS)
		{
		if (nargs > 3)
			except("usage: Server(name = .Name, port = .Port, exit = False)");
		SuObject* selfob = self.object();
		static Value Name("Name");
		static Value Port("Port");
		static Value Exit("Exit");
		Value name = selfob->getdata(Name);
		Value port = selfob->getdata(Port);
		Value exit = SuBoolean::f;
		int unamed = nargs - nargnames;
		if (unamed >= 1)
			name = ARG(0);
		if (unamed >= 2)
			port = ARG(1);
		if (unamed >= 3)
			exit = ARG(2);
		static int aname = ::symnum("name");
		static int aport = ::symnum("port");
		static int aexit = ::symnum("exit");
		for (int i = 0; i < nargnames; ++i)
			{
			if (argnames[i] == aname)
				name = ARG(unamed + i);
			else if (argnames[i] == aport)
				port = ARG(unamed + i);
			else if (argnames[i] == aexit)
				exit = ARG(unamed + i);
			}

		socketServer(name.str(), port.integer(), suserver, self, exit == SuTrue);
		}
	return Value();
	}

Value suSocketServer()
	{
	static SuSocketServer* suss = new SuSocketServer();
	return suss;
	}

// SuServerInstance & Server ========================================

class Server;

class SuServerInstance : public SuObject
	{
public:
	SuServerInstance(SocketConnect* s) : sc(s)
		{ }
	void out(Ostream& os);
	Value call(Value self, Value member, short nargs, short nargnames, 
		ushort* argnames, int each);
	SocketConnect* sc;
	};

static void _stdcall suserver(void* arg)
	{
	SocketConnect* sc = 0;
	try
		{
		Proc p; tss_proc() = &p;

		sc = (SocketConnect*) arg;
		SuServerInstance* ob = new SuServerInstance(sc);
		ob->myclass = (SuValue*) sc->getarg();
		ob->call(ob, NEW, 0, 0, 0, -1);
		static Value RUN("Run");
		ob->call(ob, RUN, 0, 0, 0, -1);
		}
	catch(const Except& x)
		{
		errlog("exception in SocketServer: " , x.exception);
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
	catch(const Except& x)
		{
		errlog("exception closing SocketServer connection: " , x.exception);
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
		SuString* s = new SuString(n);
		if (! sc->read(s->buf(), n))
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
