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
#include "sumethod.h"
#include "sudate.h"
#include "surecord.h"
#include "sudb.h"
#include "random.h"
#include "dbms.h"
#include "construct.h"
#include <time.h> // for time for srand
#include "checksum.h"
#include "susockets.h"
#include "type.h"
#include "callback.h"
#include "prim.h"
#include "fatal.h"
#include "gc.h"
#include "errlog.h"
#include "port.h"
#include "fibers.h"
#include "win.h"
#include "msgloop.h"
#include "exceptimp.h"

Value run(const char* s)
	{
	return docall(compile(CATSTR3("function () {\n", s, "\n}")), CALL);
	}

char* eval(const char* s)
	{
	char* str;
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

// primitives -------------------------------------------------------

Value su_msgloop()
	{
	int nargs = 1;
	HWND hwnd = (HWND) ARG(0).integer();
	message_loop(hwnd);
	return Value();
	}
PRIM(su_msgloop, "MessageLoop(hdlg)");

// for compatibility with old stdlib
Value su_setaccels()
	{
	int nargs = 2;
	SetWindowLong((HWND) ARG(0).integer(), GWL_USERDATA,  ARG(1).integer());
	return Value();
	}
PRIM(su_setaccels, "SetAccelerators(hwnd, haccels)");

Value su_exit()
	{
	const int nargs = 1;
	if (ARG(0) == SuTrue)
		exit(0);
	PostQuitMessage(ARG(0).integer());
	return Value();
	}
PRIM(su_exit, "Exit(status = 0)");

Value errorlog()
	{
	const int nargs = 1;
	errlog(ARG(0).str());
	return Value();
	}
PRIM(errorlog, "ErrorLog(string)");

#ifdef _MSC_VER
#pragma warning(disable:4717) // stack overflow
#endif
Value stackoverflow()
	{
	return stackoverflow();
	}
PRIM(stackoverflow, "StackOverflow()");

Value trace()
	{
	const int nargs = 2;
	int prev_trace_level = trace_level;
	if (ARG(0).int_if_num(&trace_level))
		{
		if (0 == (trace_level & (TRACE_CONSOLE | TRACE_LOGFILE)))
			trace_level |= TRACE_CONSOLE | TRACE_LOGFILE;
		}
	else
		{
		if (val_cast<SuString*>(ARG(0)))
			tout() << ARG(0).gcstr() << endl;
		else
			tout() << ARG(0) << endl;
		}
	Value block = ARG(1);
	if (block != SuFalse)
		{
		try
			{
			KEEPSP
			Value result = block.call(block, CALL, 0, 0, 0, -1);
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
PRIM(trace, "Trace(value, block = false)");

Value gcdump()
	{
	GC_dump();
	return Value();
	}
PRIM(gcdump, "GC_dump()");

Value gccollect()
	{
	GC_gcollect();
	return Value();
	}
PRIM(gccollect, "GC_collect()");

Value display()
	{
	OstreamStr os;
	os << TOP();
	return new SuString(os.str());
	}
PRIM(display, "Display(value)");

struct Synch
	{
	Synch()
		{ ++tss_proc()->synchronized; }
	~Synch()
		{ --tss_proc()->synchronized; }
	};

Value synchronized()
	{
	const int nargs = 1;
	KEEPSP
	Synch synch;
	return ARG(0).call(ARG(0), CALL, 0, 0, 0, -1);
	}
PRIM(synchronized, "Synchronized(block)");

Value frame()
	{
	int i = 1 + abs(TOP().integer()); // + 1 to skip this frame
	if (tss_proc()->fp - i <= tss_proc()->frames)
		return SuBoolean::f;
	if (tss_proc()->fp[-i].fn)
		return tss_proc()->fp[-i].fn;
	else
		return tss_proc()->fp[-i].prim;
	}
PRIM(frame, "Frame(offset)");

Value locals()
	{
	static Value SYM_THIS("this");

	int i = 1 + abs(TOP().integer()); // + 1 to skip this frame
	if (tss_proc()->fp - i < tss_proc()->frames)
		return SuBoolean::f;
	Frame& frame = tss_proc()->fp[-i];
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
PRIM(locals, "Locals(offset)");

Value buffer()
	{
	const int nargs = 2;
	int n;
	if (! ARG(0).int_if_num(&n))
		except("usage: Buffer(size, string=\"\")");
	if (n < 0)
		except("invalid Buffer size: " << n);
	SuBuffer* b = new SuBuffer(n, ARG(1).gcstr());
	return b;
	}
PRIM(buffer, "Buffer(size, string='')");

Value unload()
	{
	gcstring s = TOP().gcstr();
	// TODO: check for valid name
	globals.put(s.str(), Value());
	return Value();
	}
PRIM(unload, "Unload(name)");

Value objectq()
	{
	Value x = TOP();
	return x.ob_if_ob() && ! val_cast<SuClass*>(x)
		? SuTrue : SuFalse;
	}
PRIM(objectq, "Object?(value)");

Value classq()
	{
	return val_cast<SuClass*>(TOP())
		? SuTrue : SuFalse;
	}
PRIM(classq, "Class?(value)");

Value numberq()
	{
	Value x = TOP();
	return x.is_int() || val_cast<SuNumber*>(x)
		? SuTrue : SuFalse;
	}
PRIM(numberq, "Number?(value)");

Value stringq()
	{
	return val_cast<SuString*>(TOP())
		? SuTrue : SuFalse;
	}
PRIM(stringq, "String?(value)");

Value booleanq()
	{
	return val_cast<SuBoolean*>(TOP())
		? SuTrue : SuFalse;
	}
PRIM(booleanq, "Boolean?(value)");

Value functionq()
	{
	Value x = TOP();
	return val_cast<Func*>(x) || val_cast<SuMethod*>(x)
		? SuTrue : SuFalse;
	}
PRIM(functionq, "Function?(value)");

Value dateq()
	{
	return val_cast<SuDate*>(TOP())
		? SuTrue : SuFalse;
	}
PRIM(dateq, "Date?(value)");

Value recordq()
	{
	return val_cast<SuRecord*>(TOP())
		? SuTrue : SuFalse;
	}
PRIM(recordq, "Record?(value)");

Value name()
	{
	if (Named* named = TOP().get_named())
		return new SuString(named->name());
	else
		return SuString::empty_string;
	}
PRIM(name, "Name(value)");

Value memcopy()
	{
	const int nargs = 2;
	if (SuString* s = val_cast<SuString*>(ARG(0)))
		{
		// Memcopy(string, address)
		char* p = (char*) ARG(1).integer();
		memcpy(p, s->buf(), s->size());
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
PRIM(memcopy, "Memcopy(address, size)");

Value su_string_from()
	{
	const int nargs = 1;
	char* s = (char*) ARG(0).integer();
	return new SuString(s);
	}
PRIM(su_string_from, "StringFrom(address)");

Value su_cmdline()
	{
	extern char* cmdline;
	return new SuString(cmdline);
	}
PRIM(su_cmdline, "Cmdline()");

Value make_class()
	{
	const int nargs = 2;
	SuObject* ob = ARG(0).object();
	short base = globals(ARG(1).str());
	SuClass *c = new SuClass(base);
	c->set_members(ob);
	c->set_readonly();
	return c;
	}
PRIM(make_class, "Class(object, class='Object')");

Value su_random()
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
PRIM(su_random, "Random(range)");

static void _stdcall thread(void* arg)
	{
	Proc p; tss_proc() = &p;

	try
		{
		Value fn = (SuValue*) arg;
		fn.call(fn, CALL, 0, 0, 0, -1);
		}
	catch (const Except& e)
		{
		MessageBox(0, e.str(), "Error in Thread", MB_TASKMODAL | MB_OK);
		}

	extern Dbms*& tss_thedbms();
	delete tss_thedbms();
	tss_thedbms() = 0;

	Fibers::end();
	}

Value su_thread()
	{
	const int nargs = 1;
	Fibers::create(thread, ARG(0).ptr());
	return Value();
	}
PRIM(su_thread, "Thread(function)");

Value su_timestamp()
	{
	return dbms()->timestamp();
	}
PRIM(su_timestamp, "Timestamp()");

Value su_dump()
	{
	const int nargs = 1;
	dbms()->dump(ARG(0).str());
	return Value();
	}
PRIM(su_dump, "Dump(table='')");

Value su_backup()
	{
	const int nargs = 1;
	dbms()->copy(ARG(0).str());
	return Value();
	}
PRIM(su_backup, "Backup(filename='suneido.db.backup')");

Value su_serverq()
	{
	extern bool is_server;
	return is_server ? SuTrue : SuFalse;
	}
PRIM(su_serverq, "Server?()");

Value su_serverip()
	{
	return new SuString(get_dbms_server_ip());
	}
PRIM(su_serverip, "ServerIP()");

Value su_serverport()
	{
	extern int su_port;
	return su_port;
	}
PRIM(su_serverport, "ServerPort()");

Value su_exepath()
	{
	char exefile[1024];
	get_exe_path(exefile, sizeof exefile);
	return new SuString(exefile);
	}
PRIM(su_exepath, "ExePath()");

Value su_built()
	{
	extern char* build_date;
	return new SuString(build_date);
	}
PRIM(su_built, "Built()");

#include <process.h>

Value su_system()
	{
	const int nargs = 1;
	char* cmd = ARG(0) == Value(0) ? NULL : ARG(0).str();
	return system(cmd);
	}
PRIM(su_system, "System(command)");

Value su_spawn()
	{
	const int nargs = 3;
	return spawnlp(ARG(0).integer(),
		ARG(1).str(), ARG(1).str(), ARG(2).str(), NULL);
	}
PRIM(su_spawn, "Spawn(mode, command, args='')");

#include "md5.h"

Value su_md5()
	{
	const int nargs = 1;
	gcstring in = ARG(0).gcstr();
	SuString* out = new SuString(16);
	return md5(in.buf(), in.size(), out->buf()) ? (Value) out : (Value) SuFalse;
	}
PRIM(su_md5, "Md5(string)");

Value su_eq()
	{
	const int nargs = 2;
	return ARG(0) == ARG(1) ? SuTrue : SuFalse;
	}
PRIM(su_eq, "Eq(x, y)");

Value su_neq()
	{
	const int nargs = 2;
	return ARG(0) != ARG(1) ? SuTrue : SuFalse;
	}
PRIM(su_neq, "Neq(x, y)");

Value su_lt()
	{
	const int nargs = 2;
	return ARG(0) < ARG(1) ? SuTrue : SuFalse;
	}
PRIM(su_lt, "Lt(x, y)");

Value su_lte()
	{
	const int nargs = 2;
	return ARG(0) <= ARG(1) ? SuTrue : SuFalse;
	}
PRIM(su_lte, "Lte(x, y)");

Value su_gt()
	{
	const int nargs = 2;
	return ARG(0) > ARG(1) ? SuTrue : SuFalse;
	}
PRIM(su_gt, "Gt(x, y)");

Value su_gte()
	{
	const int nargs = 2;
	return ARG(0) >= ARG(1) ? SuTrue : SuFalse;
	}
PRIM(su_gte, "Gte(x, y)");

Value su_neg()
	{
	const int nargs = 1;
	return -ARG(0);
	}
PRIM(su_neg, "Neg(x)");

Value su_add()
	{
	const int nargs = 2;
	return ARG(0) + ARG(1);
	}
PRIM(su_add, "Add(x, y)");

Value su_sub()
	{
	const int nargs = 2;
	return ARG(0) - ARG(1);
	}
PRIM(su_sub, "Sub(x, y)");

Value su_mul()
	{
	const int nargs = 2;
	return ARG(0) * ARG(1);
	}
PRIM(su_mul, "Mul(x, y)");

Value su_div()
	{
	const int nargs = 2;
	return ARG(0) / ARG(1);
	}
PRIM(su_div, "Div(x, y)");

Value su_mod()
	{
	const int nargs = 2;
	return ARG(0).integer() % ARG(1).integer();
	}
PRIM(su_mod, "Mod(x, y)");

Value su_cat()
	{
	const int nargs = 2;
	return new SuString(ARG(0).gcstr() + ARG(1).gcstr());
	}
PRIM(su_cat, "Cat(x, y)");

#include "regexp.h"

Value su_match()
	{
	const int nargs = 2;
	gcstring sy = ARG(0).gcstr();
	gcstring sx = ARG(1).gcstr();
	return rx_match(sx.buf(), sx.size(), rx_compile(sy))
		? SuTrue : SuFalse;
	}
PRIM(su_match, "Match(x, y)");

Value su_nomatch()
	{
	return su_match() == SuTrue ? SuFalse : SuTrue;
	}
PRIM(su_nomatch, "NoMatch(x, y)");

bool forcebool(Value val)
	{
	if (val == SuTrue)
		return true;
	else if (val == SuFalse)
		return false;
	else
		except("true or false expected, got: " << val);
	}

Value su_and()
	{
	const int nargs = 2;
	return forcebool(ARG(0)) && forcebool(ARG(1));
	}
PRIM(su_and, "And(x, y)");

Value su_or()
	{
	const int nargs = 2;
	return forcebool(ARG(0)) || forcebool(ARG(1));
	}
PRIM(su_or, "Or(x, y)");

Value su_not()
	{
	const int nargs = 1;
	return forcebool(ARG(0)) ? SuFalse : SuTrue;
	}
PRIM(su_not, "Not(x)");

Value su_pack()
	{
	const int nargs = 1;
	int len = ARG(0).packsize();
	SuString* s = new SuString(len);
	ARG(0).pack(s->buf());
	return s;
	}
PRIM(su_pack, "Pack(value)");

#include "pack.h"

Value su_unpack()
	{
	const int nargs = 1;
	gcstring s = ARG(0).gcstr();
	return unpack(s);
	}
PRIM(su_unpack, "Unpack(string)");

Value su_memory_arena()
	{
	return GC_get_heap_size();
	}
PRIM(su_memory_arena, "MemoryArena()");

bool autowaitcursor = true; // used in awcursor.h

Value su_nowaitcursor()
	{
	autowaitcursor = false;
	return Value();
	}
PRIM(su_nowaitcursor, "NoWaitCursor()");

Value su_type()
	{
	const int nargs = 1;
	return new SuString(ARG(0).type());
	}
PRIM(su_type, "Type(value)");

Value su_getenv()
	{
	const int nargs = 1;
	char* s = getenv(ARG(0).str());
	return new SuString(s ? s : "");
	}
PRIM(su_getenv, "Getenv(string)");

Value su_winerr()
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
PRIM(su_winerr, "WinErr(number)");

Value su_unixtime()
	{
	return new SuNumber(time(NULL));
	}
PRIM(su_unixtime, "UnixTime()");

// rich edit --------------------------------------------------------
#include "rich.h"

Value su_richedit_ole()
	{
	const int nargs = 1;
	RichEditOle(ARG(0).integer());
	return Value();
	}
PRIM(su_richedit_ole, "RichEditOle(hwnd)");

Value su_richedit_get()
	{
	const int nargs = 1;
	return new SuString(RichEditGet((HWND) ARG(0).integer()));
	}
PRIM(su_richedit_get, "RichEditGet(hwnd)");

Value su_richedit_put()
	{
	const int nargs = 2;
	RichEditPut((HWND) ARG(0).integer(), ARG(1).gcstr());
	return Value();
	}
PRIM(su_richedit_put, "RichEditPut(hwnd, string)");

// mkrec - calls generated by compiler to handle partially literal [...]

class MkRec : public Func
	{
public:
	MkRec()
		{ named.num = globals("MkRec"); }
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
	};

Value MkRec::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
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

// ------------------------------------------------------------------

Value su_ScintillaStyle();
Value su_seq();
Value su_exit();

Value NEW;
Value CALL;
Value CALL_CLASS;
Value CALL_INSTANCE;
Value INSTANTIATE;
Value SuOne;
Value SuZero;
Value SuMinusOne;
Value SuTrue;
Value SuFalse;
Value SuEmptyString;

Value root_class;

void builtin(int gnum, Value value); // in library.cpp
void builtin(char* name, Value value)
	{
	builtin(globals(name), value);
	}

const int MAXPRIMS = 100;
int nprims = 0;
Prim* prims[MAXPRIMS];

Prim::Prim(PrimFn f, char* d) : fn(f), decl(d)
	{
	if (nprims >= MAXPRIMS)
		fatal("too many primitives - increase MAXPRIMS in builtins.cpp");
	prims[nprims++] = this;
	}

void prim(char* name, Primitive* prim)
	{
	prim->named.num = globals(name);
	builtin(prim->named.num, prim);
	}

void builtins()
	{
	verify(FalseNum == globals("False"));
	verify(TrueNum == globals("True"));
	verify(OBJECT == globals("Object"));

	INSTANTIATE = Value("instantiate");
	CALL = Value("call");				// Note: no class can have a "call" method
	CALL_CLASS = Value("CallClass");	// CALL on Class becomes CALL_CLASS
	CALL_INSTANCE = Value("Call");		// CALL on SuObject becomes CALL_INSTANCE
	NEW = Value("New");
	SuOne = Value(1);
	SuZero = Value(0);
	SuMinusOne = Value(-1);
	SuTrue = SuBoolean::t;
	SuFalse = SuBoolean::f;
	SuEmptyString = SuString::empty_string;

	// struct, dll, callback types
	builtin("bool", new TypeBool);
	builtin("char", new TypeInt<char>);
	builtin("short", new TypeInt<short>);
	builtin("long", new TypeInt<long>);
	builtin("int64", new TypeInt<int64>);
	builtin("float", new TypeFloat);
	builtin("double", new TypeDouble);
	builtin("handle", new TypeWinRes<SuHandle>);
	builtin("gdiobj", new TypeWinRes<SuGdiObj>);
	builtin("string", new TypeString);
	builtin("instring", new TypeString(true));
	builtin("buffer", new TypeBuffer);
	builtin("resource", new TypeResource);

	builtin("mkrec", new MkRec);
	builtin("False", SuFalse);
	builtin("True", SuTrue);
	builtin("Object", root_class = new RootClass);
	builtin("Suneido", new SuObject);
	builtin("Construct", new Construct);
	builtin("Construct2", new Instance);
	builtin("Database", new DatabaseClass);
	builtin("Transaction", new TransactionClass);
	builtin("Cursor", new CursorClass);
	builtin("Record", new SuRecordClass);
	builtin("Date", new SuDateClass);
	extern Value su_file();
	builtin("File", su_file());
	extern Value su_adler32();
	builtin("Adler32", su_adler32());
	builtin("SocketServer", suSocketServer());
	extern Value su_image();
	builtin("Image", su_image());
	extern Value su_runpiped();
	builtin("RunPiped", su_runpiped());
	extern Value su_Query1();
	builtin("Query1", su_Query1());
	extern Value su_QueryFirst();
	builtin("QueryFirst", su_QueryFirst());
	extern Value su_QueryLast();
	builtin("QueryLast", su_QueryLast());
	extern Value su_ServerEval();
	builtin("ServerEval", su_ServerEval());

	for (int i = 0; i < nprims; ++i)
		{
		Primitive* p = new Primitive(prims[i]->decl, prims[i]->fn);
		builtin(p->named.num, p);
		}
	}
