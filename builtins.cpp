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

#include "std.h"
#include "value.h"
#include "compile.h"
#include "catstr.h"
#include "ostreamstr.h"
#include "interp.h"
#include "globals.h"
#include "sustring.h"
#include "sunumber.h"
#include "suboolean.h"
#include "sufunction.h"
#include "suobject.h"
#include "suclass.h"
#include "suinstance.h"
#include "sumethod.h"
#include "sudate.h"
#include "surecord.h"
#include "sudb.h"
#include "random.h"
#include "dbms.h"
#include "construct.h"
#include <ctime> // for time for srand
#include "type.h"
#include "builtin.h"
#include "fatal.h"
#include "gc.h"
#include "errlog.h"
#include "port.h"
#include "fibers.h"
#include "win.h"
#include "msgloop.h"
#include "exceptimp.h"
#include "trace.h"
#include "build.h"
#include "suwinres.h"

Value run(const char* s)
	{
	return docall(compile(CATSTR3("function () {\n", s, "\n}")), CALL);
	}

const char* eval(const char* s)
	{
	const char* str;
	try
		{
		str = run(s).str();
		}
	catch (const Except& e)
		{
		OstreamStr oss;
		oss << "eval(" << s << ") => " << e;
		str = oss.str();
		}
	return str;
	}

// built-in functions -----------------------------------------------

BUILTIN(Fatal, "(s = '')")
	{
	const int nargs = 1;
	fatal(ARG(0).str());
}

BUILTIN(MessageLoop, "(hdlg)")
	{
	int nargs = 1;
	HWND hwnd = (HWND) ARG(0).integer();
	message_loop(hwnd);
	return Value();
	}

BUILTIN(Exit, "(status = 0)")
	{
	const int nargs = 1;
	if (ARG(0) == SuTrue)
		exit(0);
	PostQuitMessage(ARG(0).integer());
	return Value();
	}

BUILTIN(ErrorLog, "(string)")
	{
	const int nargs = 1;
	errlog(ARG(0).str());
	return Value();
	}

#if defined(__clang__)
#pragma clang diagnostic ignored "-Winfinite-recursion"
#elif defined(_MSC_VER)
#pragma warning(disable:4717) // stack overflow
#endif
BUILTIN(StackOverflow, "()")
	{
	return su_StackOverflow();
	}

BUILTIN(Trace, "(value, block = false)")
	{
	const int nargs = 2;
	int prev_trace_level = trace_level;
	if (val_cast<SuString*>(ARG(0)))
		tout() << ARG(0).gcstr() << endl;
	else
		{
		trace_level = ARG(0).integer();
		if (0 == (trace_level & (TRACE_CONSOLE | TRACE_LOGFILE)))
			trace_level |= TRACE_CONSOLE | TRACE_LOGFILE;
		}
	Value block = ARG(1);
	if (block != SuFalse)
		{
		try
			{
			KEEPSP
			Value result = block.call(block, CALL);
			trace_level = prev_trace_level;
			return result;
			}
		catch (const Except&)
			{
			trace_level = prev_trace_level;
			throw ;
			}
		}
	return Value();
	}

BUILTIN(GC_dump, "()")
	{
	GC_dump();
	return Value();
	}

BUILTIN(GC_collect, "()")
	{
	GC_gcollect();
	return Value();
	}

BUILTIN(Display, "(value)")
	{
	OstreamStr os;
	os << TOP();
	return new SuString(os.str());
	}

struct Synch
	{
	Synch()
		{ ++tls().synchronized; }
	~Synch()
		{ --tls().synchronized; }
	};

BUILTIN(Synchronized, "(block)")
	{
	const int nargs = 1;
	KEEPSP
	Synch synch;
	return ARG(0).call(ARG(0), CALL);
	}

BUILTIN(Frame, "(offset)")
	{
	int i = 1 + abs(TOP().integer()); // + 1 to skip this frame
	if (tls().proc->fp - i <= tls().proc->frames)
		return SuFalse;
	if (tls().proc->fp[-i].fn)
		return tls().proc->fp[-i].fn;
	else
		return tls().proc->fp[-i].prim;
	}

BUILTIN(Locals, "(offset)")
	{
	static Value SYM_THIS("this");

	int i = 1 + abs(TOP().integer()); // + 1 to skip this frame
	if (tls().proc->fp - i < tls().proc->frames)
		return SuFalse;
	Frame& frame = tls().proc->fp[-i];
	SuObject* ob = new SuObject();
	ob->put(SYM_THIS, frame.self);
	if (frame.fn)
		{
		for (i = 0; i < frame.fn->nlocals; ++i)
			if (frame.local[i])
				ob->put(symbol(frame.fn->locals[i]), frame.local[i]);
		}
	return ob;
	}

BUILTIN(Buffer, "(size, string='')")
	{
	const int nargs = 2;
	int n;
	if (! ARG(0).int_if_num(&n))
		except("usage: Buffer(size, string=\"\")");
	if (n <= 0)
		except("Buffer size must be greater than zero");
	return new SuBuffer(n, ARG(1).gcstr());
	}

BUILTIN(ObjectQ, "(value)")
	{ //TODO don't include instances
	Value x = TOP();
	return x.ob_if_ob() || val_cast<SuInstance*>(x)
		? SuTrue : SuFalse;
	}

BUILTIN(ClassQ, "(value)")
	{
	return val_cast<SuClass*>(TOP())
		? SuTrue : SuFalse;
	}

BUILTIN(NumberQ, "(value)")
	{
	Value x = TOP();
	return x.is_int() || val_cast<SuNumber*>(x)
		? SuTrue : SuFalse;
	}

BUILTIN(StringQ, "(value)")
	{
	return val_cast<SuString*>(TOP())
		? SuTrue : SuFalse;
	}

BUILTIN(BooleanQ, "(value)")
	{
	return val_cast<SuBoolean*>(TOP())
		? SuTrue : SuFalse;
	}

BUILTIN(FunctionQ, "(value)")
	{
	Value x = TOP();
	return val_cast<Func*>(x) || val_cast<SuMethod*>(x)
		? SuTrue : SuFalse;
	}

BUILTIN(DateQ, "(value)")
	{
	return val_cast<SuDate*>(TOP())
		? SuTrue : SuFalse;
	}

BUILTIN(RecordQ, "(value)")
	{
	return val_cast<SuRecord*>(TOP())
		? SuTrue : SuFalse;
	}

BUILTIN(Name, "(value)")
	{
	if (auto named = TOP().get_named())
		return new SuString(named->name());
	else
		return SuEmptyString;
	}

BUILTIN(Memcopy, "(address, size)")
	{
	const int nargs = 2;
	if (SuString* s = val_cast<SuString*>(ARG(0)))
		{
		// Memcopy(string, address)
		char* p = (char*) ARG(1).integer();
		memcpy(p, s->ptr(), s->size());
		return Value();
		}
	else
		{
		// Memcopy(address, size) => string
		char* p = (char*) ARG(0).integer();
		int n = ARG(1).integer();
		return new SuString(p, n);
		}
	}

BUILTIN(StringFrom, "(address)")
	{
	const int nargs = 1;
	char* s = (char*) ARG(0).integer();
	return new SuString(s);
	}

BUILTIN(Cmdline, "()")
	{
	extern const char* cmdline;
	return new SuString(cmdline);
	}

BUILTIN(Random, "(limit)")
	{
	static bool first = true;
	if (first)
		{
		first = false;
		srand((unsigned) time(NULL));
		}
	int limit = TOP().integer();
	return limit == 0 ? 0 : random(limit);
	}

BUILTIN(Timestamp, "()")
	{
	return dbms()->timestamp();
	}

extern bool is_client, is_server;

BUILTIN(ServerQ, "()")
	{
	return is_server ? SuTrue : SuFalse;
	}

BUILTIN(ServerIP, "()")
	{
	return new SuString(get_dbms_server_ip());
	}

BUILTIN(ServerPort, "()")
	{
	extern int su_port;
	return (is_client || is_server) ? su_port : SuEmptyString;
	}

BUILTIN(ExePath, "()")
	{
	char exefile[1024];
	get_exe_path(exefile, sizeof exefile);
	return new SuString(exefile);
	}

BUILTIN(Built, "()")
	{
	return new SuString(build);
	}

#include <process.h>

BUILTIN(System, "(command)")
	{
	const int nargs = 1;
	const char* cmd = ARG(0) == Value(0) ? nullptr : ARG(0).str();
	return system(cmd);
	}

BUILTIN(Pack, "(value)")
	{
	const int nargs = 1;
	int len = ARG(0).packsize();
	char* buf = salloc(len);
	ARG(0).pack(buf);
	return SuString::noalloc(buf, len);
	}

#include "pack.h"

BUILTIN(Unpack, "(string)")
	{
	const int nargs = 1;
	gcstring s = ARG(0).gcstr();
	return unpack(s);
	}

BUILTIN(MemoryArena, "()")
	{
	return GC_get_heap_size();
	}

BUILTIN(Type, "(value)")
	{
	const int nargs = 1;
	return new SuString(ARG(0).type());
	}

BUILTIN(Getenv, "(string)")
	{
	const int nargs = 1;
	char* s = getenv(ARG(0).str());
	return s ? new SuString(s) : SuEmptyString;
	}

BUILTIN(WinErr, "(number)")
	{
	const int nargs = 1;
	void *buf;
	int n = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, ARG(0).integer(), 0,
		(LPTSTR) &buf, 0, NULL);
	gcstring s(n ? (char*) buf : "");
	LocalFree(buf);
	return new SuString(s.trim());
	}

BUILTIN(UnixTime, "()")
	{
	return new SuNumber(time(NULL));
	}

static Value call(Value fn)
	{
	KEEPSP
	return fn.call(fn, CALL);
	}

BUILTIN(Finally, "(main_block, final_block)")
	{
	const int nargs = 2;
	Value main_block = ARG(0);
	Value finally_block = ARG(1);
	try
		{
		Value result = call(main_block);
		call(finally_block); // could throw
		return result;
		}
	catch (...)
		{
		try
			{
			call(finally_block);
			}
		catch (...)
			{
			// ignore exception from final_block if main_block threw
			}
		throw;
		}
	}

BUILTIN(SameQ, "(x, y)")
	{
	const int nargs = 2;
	return ARG(0).sameAs(ARG(1)) ? SuTrue : SuFalse;
	}

BUILTIN(Hash, "(value)")
	{
	const int nargs = 1;
	return ARG(0).hash();
	}

#define RC(name) \
	extern int name##_count; \
	if (name##_count) \
		ob->put(#name, name##_count)

BUILTIN(ResourceCounts, "()")
	{
	auto ob = new SuObject();
	RC(handle);
	RC(gdiobj);
	RC(File);
	RC(RunPiped);
	RC(Md5);
	RC(Sha1);
	RC(Sha256);
	RC(socket);
	return ob;
	}

// rich edit --------------------------------------------------------
#include "rich.h"

BUILTIN(RichEditOle, "(hwnd)")
	{
	const int nargs = 1;
	RichEditOle(ARG(0).integer());
	return Value();
	}

BUILTIN(RichEditGet, "(hwnd)")
	{
	const int nargs = 1;
	return new SuString(RichEditGet((HWND) ARG(0).integer()));
	}

BUILTIN(RichEditPut, "(hwnd, string)")
	{
	const int nargs = 2;
	RichEditPut((HWND) ARG(0).integer(), ARG(1).gcstr());
	return Value();
	}

// spawn

#include "builtinargs.h"

class Spawn : public Func
	{
public:
	Spawn()
		{
		named.num = globals("Spawn");
		// so params works
		nparams = 3;
		rest = true;
		locals = new ushort[3];
		locals[0] = ::symnum("mode");
		locals[1] = ::symnum("command");
		locals[2] = ::symnum("args");
		}
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override
		{
		if (member != CALL)
			return Func::call(self, member, nargs, nargnames, argnames, each);
		BuiltinArgs args(nargs, nargnames, argnames, each);
		args.usage("Spawn(mode, command, @args)");
		int mode = args.getint("mode");
		auto cmd = args.getstr("command");
		auto argv = new const char*[nargs];
		int i = 0;
		argv[i++] = cmd;
		while (Value arg = args.getNextUnnamed())
			{
			argv[i++] = arg.str();
			}
		argv[i] = nullptr;
		return _spawnvp(mode, cmd, argv);
		}
	const char* type() const override
		{
		return "Builtin";
		}
	};

// mkrec - calls generated by compiler to handle partially literal [...]

class MkRec : public Func
	{
public:
	MkRec()
		{ named.num = globals("MkRec"); }
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;
	};

Value MkRec::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	// pre: last argument is the literal record
	{
	Value* args = GETSP() - nargs + 1;
	Value lits = args[--nargs];
	if (nargnames)
		--nargnames;
	SuRecord* ob = new SuRecord(*val_cast<SuRecord*>(lits));
	// TODO: copy-on-write if no args (only literals)

	int ai = 0;
	const int unamed = nargs - nargnames;
	for (int i = 0; ai < unamed; ++i)
		if (! ob->has(i))
			ob->put(i, args[ai++]);

	for (int j = 0; ai < nargs; ++ai, ++j)
		ob->put(symbol(argnames[j]), args[ai]);

	return ob;
	}

// rangeTo - calls generated by compiler to handle x[i .. j]
class RangeTo : public Func
	{
public:
	RangeTo()
		{ named.num = globals("RangeTo"); }
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override
		{
		return ARG(0).rangeTo(ARG(1).integer(), ARG(2).integer());
		}
	};

// rangeLen - calls generated by compiler to handle x[i .. j]
class RangeLen : public Func
	{
public:
	RangeLen()
		{ named.num = globals("RangeLen"); }
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override
		{
		int len = ARG(2).integer();
		if (len < 0)
			len = 0;
		return ARG(0).rangeLen(ARG(1).integer(), len);
		}
	};

// ------------------------------------------------------------------

Value NEW;
Value CALL;
Value PARAMS;
Value CALL_CLASS;
Value CALL_INSTANCE;
Value INSTANTIATE;
Value SuOne;
Value SuZero;
Value SuMinusOne;
Value SuTrue;
Value SuFalse;
Value SuEmptyString;

void builtin(int gnum, Value value); // in library.cpp

void builtin(const char* name, Value value)
	{
	builtin(globals(name), value);
	}

#define BUILTIN_CLASS(name, clazz) extern Value clazz(); builtin(name, clazz());

void builtins()
	{
	globals(""); // don't use the [0] slot

	INSTANTIATE = Value("instantiate");
	CALL = Value("call");				// Note: no class can have a "call" method
	PARAMS = Value("Params");
	CALL_CLASS = Value("CallClass");	// CALL on Class becomes CALL_CLASS
	CALL_INSTANCE = Value("Call");		// CALL on SuObject becomes CALL_INSTANCE
	NEW = Value("New");
	SuOne = Value(1);
	SuZero = Value(0);
	SuMinusOne = Value(-1);
	SuTrue = SuBoolean::t;
	SuFalse = SuBoolean::f;
	SuEmptyString = symbol("");

	// struct, dll, callback types
	builtin("bool", new TypeBool);

	// TODO remove char/short/long once switched over to int8/16/32/64
	builtin("char", new TypeInt<char>);
	builtin("short", new TypeInt<short>);
	builtin("long", new TypeInt<long>);

	builtin("int8", new TypeInt<char>);
	builtin("int16", new TypeInt<short>);
	builtin("int32", new TypeInt<long>);
	builtin("int64", new TypeInt<long long>);
	builtin("pointer", new TypeOpaquePointer);
	builtin("float", new TypeFloat);
	builtin("double", new TypeDouble);
	builtin("handle", new TypeWinRes<SuHandle>);
	builtin("gdiobj", new TypeWinRes<SuGdiObj>);
	builtin("string", new TypeString);
	builtin("instring", new TypeString(true));
	builtin("buffer", new TypeBuffer);
	builtin("resource", new TypeResource);

	builtin("rangeTo", new RangeTo);
	builtin("rangeLen", new RangeLen);
	builtin("mkrec", new MkRec);
	builtin("Object", su_object());
	builtin("Suneido", new SuObject);

	builtin("Construct", new Construct);
	builtin("Transaction", new TransactionClass);
	builtin("Cursor", new CursorClass);
	builtin("Record", su_record());
	builtin("Date", new SuDateClass);
	builtin("Spawn", new Spawn);

	BUILTIN_CLASS("Database", su_Database);
	BUILTIN_CLASS("File", su_file);
	BUILTIN_CLASS("Adler32", su_adler32);
	BUILTIN_CLASS("Md5", su_md5);
	BUILTIN_CLASS("Sha1", su_sha1);
	BUILTIN_CLASS("Sha256", su_sha256);
	BUILTIN_CLASS("SocketServer", suSocketServer);
	BUILTIN_CLASS("RunPiped", su_runpiped);
	BUILTIN_CLASS("Query1", su_Query1);
	BUILTIN_CLASS("QueryFirst", su_QueryFirst);
	BUILTIN_CLASS("QueryLast", su_QueryLast);
	BUILTIN_CLASS("ServerEval", su_ServerEval);
	BUILTIN_CLASS("Scanner", su_scanner);
	BUILTIN_CLASS("QueryScanner", su_queryscanner);
	BUILTIN_CLASS("Thread", su_Thread);

	install_builtin_functions();
	}
