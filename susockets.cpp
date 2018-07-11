// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "susockets.h"
#include "sockets.h"
#include "suinstance.h"
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
#include "builtin.h"
#include "func.h"

// SuSocketClient ===================================================

class SuSocketClient : public SuValue
	{
public:
	explicit SuSocketClient(SocketConnect* s);
	void out(Ostream& os) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, short* argnames, int each) override;
	void close();
private:
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
	short nargs, short nargnames, short* argnames, int each)
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
		method_not_found("SocketClient", member);
	}

void SuSocketClient::ckopen(const char* action)
	{
	if (! sc)
		except("SocketClient: can't " << action << " a closed socket");
	}

void SuSocketClient::close()
	{
	if (sc)
		sc->close();
	sc = nullptr;
	}

BUILTIN(SocketClient, "(ipaddress, port, timeout=60, timeoutConnect=0, block=false)")
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

// SuSocketServer =====================================================
// the base for user defined Server classes

// SuSocketServer is the built-in "SocketServer" class that is derived from
class SuSocketServer : public SuClass
	{
public:
	void out(Ostream& os) const override;
	Value get(Value m) const override;
	};

void SuSocketServer::out(Ostream& os) const
	{
	os << "SocketServer /*builtin*/";
	}

static void _stdcall suserver(void* sc);

class SuServerInstance : public SuInstance
	{
public:
	explicit SuServerInstance(Value c) : SuInstance(c), sc(nullptr)
		{ }
	SuServerInstance(SuServerInstance* master, SocketConnect* s) 
		: SuInstance(*master), sc(s)
		{ } // dup
	void out(Ostream& os) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, short* argnames, int each) override;
private:
	SocketConnect* sc;
	};

class CallClass : public BuiltinFuncs
	{
public:
	Value call(Value self, Value member,
		short nargs, short nargnames, short* argnames, int each) override;
	};

Value SuSocketServer::get(Value m) const
	{
	if (m == CALL_CLASS)
		{
		static CallClass callclass;
		return &callclass;
		}
	return Value();
	}

Value CallClass::call(Value self, Value member,
	short nargs, short nargnames, short* argnames, int each)
	{
	static Value Name("Name");
	static Value Port("Port");
	static short NAME = ::symnum("name");
	static short PORT = ::symnum("port");
	static short EXIT = ::symnum("exit");

	SuClass* selfob = force<SuClass*>(self);
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("SocketServer(name = .Name, port = .Port, exit = false, ...)");
	Value name = args.getValue("name", Value());
	Value port = args.getValue("port", Value());
	Value exit = args.getValue("exit", Value());

	// convert arguments, make name, port, and exit named
	int na = 0;
	Value* a = (Value*) _alloca(sizeof (Value) * args.n_args());
	short* an = (short*) _alloca(sizeof (short) * (args.n_argnames() + 3));
	short nan = 0;
	while (Value arg = args.getNext())
		{
		a[na++] = arg;
		if (Value argname = args.curName())
			an[nan++] = argname.symnum();
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
	SuServerInstance* master = new SuServerInstance(self);
	master->call(master, NEW, na, nan, an, -1);

	socketServer(name.str(), port.integer(), suserver, master, exit.toBool());
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
	short nargs, short nargnames, short* argnames, int each)
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
		return SuInstance::call(self, member, nargs, nargnames, argnames, each);
	}

//#include "testing.h"
//#include "compile.h"
//
//TEST(susockets_server)
//	{
//	Value x = compile("SocketServer {\n"
//		"Name: Test\n"
//		"Port: 1234 }");
//	x.call(x, CALL_CLASS);
//	}
