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

#include "interp.h"
#include "compile.h"
#include "opcodes.h"
#include "suboolean.h"
#include "sunumber.h"
#include "sustring.h"
#include "except.h"
#include "exceptimp.h"
#include "regexp.h"
#include "fibers.h"
#include "sublock.h"
#include "func.h"
#include "sufunction.h"
#include "symbols.h"
#include "globals.h"
#include "suclass.h"
#include "sumethod.h"
#include "trace.h"
#include "varint.h"

short Frame::fetch_literal()
	{
	return varint(ip);
	}

Value Frame::fetch_member()
	{ 
	int n = fetch_literal();
	if (n < 0 || n >= fn->nliterals)
		except("bad member number " << "0x" << hex << n);
	return fn->literals[n];
	}

static bool catch_match(const char*, const char*);

int callnest = 0;

Value docall(Value x, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	if (trace_level & TRACE_FUNCTIONS)
		{
		tout() << "ENTER ";
		int i;
		for (i = 0; i <= callnest; ++i)
			tout() << '>';
		tout() << ' ' << x;
		if (member != CALL)
			tout() << '.' << member;
		tout() << endl;
		++callnest;
		Value y = x.call(x, member, nargs, nargnames, argnames, each);
		--callnest;
		tout() << "LEAVE ";
		for (i = 0; i <= callnest; ++i)
			tout() << '>';
		tout() << ' ' << x;
		if (member != CALL)
			tout() << '.' << member;
		tout() << endl;
		return y;
		}
	else
		return x.call(x, member, nargs, nargnames, argnames, each);
	}

// to detect: return cond ? f() : g() or: cond ? f() : f(); ...
bool Frame::jumpToPopReturn()
	{
	uchar* save_ip = ip;
	++ip;
	int jump = fetch_jump();
	uchar* target = ip + jump;
	ip = save_ip;
	return *target == I_POP || *target == I_RETURN;
	}

#define POSTCALL(nargnames, name) \
	SETSP(oldsp); \
	PUSH(result); \
	each = -1; \
	ip += nargnames * sizeof (ushort); \
	if (! TOP() && *ip != I_POP && *ip != I_RETURN && \
		(*ip != I_JUMP || ! jumpToPopReturn())) \
		except(name << " has no return value")

#define CALLX(x, member, nargs, nargnames, argnames, name) \
	oldsp = GETSP() - nargs; \
	result = docall(x, member, nargs, nargnames, argnames, each); \
	POSTCALL(nargnames, name)

#define CALLTOP(member, nargs, nargnames, argnames, name) \
	oldsp = GETSP() - nargs - 1; \
	result = docall(oldsp[1], member, nargs, nargnames, argnames, each); \
	POSTCALL(nargnames, name)

#define CALLPOP(x, member, nargs, nargnames, argnames) \
	oldsp = GETSP() - nargs; \
	docall(x, member, nargs, nargnames, argnames, each); \
	SETSP(oldsp); \
	each = -1;

#define CALLTOPPOP(member, nargs, nargnames, argnames) \
	oldsp = GETSP() - nargs - 1; \
	docall(oldsp[1], member, nargs, nargnames, argnames, each); \
	SETSP(oldsp); \
	each = -1;

#define CALLSUB(nargs, nargnames, argnames) \
	oldsp = GETSP() - nargs - 2; \
	subscript = oldsp[2]; \
	result = docall(oldsp[1], subscript, nargs, nargnames, argnames, each); \
	POSTCALL(nargnames, subscript);

#define CALLSUBSELF(nargs, nargnames, argnames) \
	oldsp = GETSP() - nargs - 1; \
	subscript = oldsp[1]; \
	result = docall(self, subscript, nargs, nargnames, argnames, each); \
	POSTCALL(nargnames, subscript);

// check top of stack for true or false
// exception if any other value
inline bool topbool()
	{
	if (TOP() == SuTrue)
		return true;
	else if (TOP() != SuFalse)
		except("conditionals require true or false, got: " << TOP());
	return false;
	}
inline bool popbool()
	{
	Value x = POP();
	if (x == SuTrue)
		return true;
	else if (x != SuFalse)
		except("conditionals require true or false, got: " << x);
	return false;
	}

Value cat(Value x, Value y)
	{
	gcstring result = x.gcstr() + y.gcstr();
	if (Except* e = val_cast<Except*>(x))
		return new Except(*e, result);
	if (Except* e = val_cast<Except*>(y))
		return new Except(*e, result);
	return new SuString(result);
	}

Frame::Frame(BuiltinFunc* p, Value s) :
	prim(p), fn(0), self(s), rule(tls().proc->fp[-1].rule), blockframe(0)
	{ }

Frame::Frame(SuFunction* f, Value s) :
	prim(0), fn(f), self(s), ip(fn->code), local(1 + GETSP() - fn->nparams),
	rule(tls().proc->fp[-1].rule), catcher(0), blockframe(0)
	{
	for (int i = fn->nparams; i < fn->nlocals; ++i)
		local[i] = Value();
	SETSP(GETSP() + (fn->nlocals - fn->nparams));
	}

// used by SuBlock::call
Frame::Frame(Frame* fp, int pc, int first, int nargs, Value s) :
	prim(0), fn(fp->fn), self(s), ip(fn->code + pc),
	local(fp->local), rule(fp->rule), catcher(0), blockframe(fp)
	{
	for (int i = nargs - 1; i >= 0; --i)
		local[first + i] = POP();
	}

Value Frame::run()
	{
	int i = 0, jump, nargs, nargnames;
	Value arg;
	Value subscript;
	Value result;
	Value* oldsp;
	Value mem;

	each = -1;
	tls().proc->super = 0;
	for (;;)
	try
		{
		if ((trace_level & TRACE_OPCODES) && *ip != I_NOP)
			fn->disasm1(tout(), ip - fn->code);
		uchar op = *ip++;
		switch (op)
			{
		case I_NOP :
			if (trace_level & TRACE_STATEMENTS)
				{
				fn->source(tout(), ip - fn->code - 1);
				tout().flush();
				}
			extern void ckinterrupt();
			ckinterrupt();
			if (! tls().proc->synchronized)
				Fibers::yieldif();
			break;
		case I_POP :
			POP();
			break ;
		case I_DUP :
			PUSH(TOP());
			break ;
		case I_SUPER :
			tls().proc->super = fetch_global();
			break ;
		case I_PUSH_LITERAL | 0 : case I_PUSH_LITERAL | 1 :
		case I_PUSH_LITERAL | 2 : case I_PUSH_LITERAL | 3 :
		case I_PUSH_LITERAL | 4 : case I_PUSH_LITERAL | 5 :
		case I_PUSH_LITERAL | 6 : case I_PUSH_LITERAL | 7 :
		case I_PUSH_LITERAL | 8 : case I_PUSH_LITERAL | 9 :
		case I_PUSH_LITERAL | 10 : case I_PUSH_LITERAL | 11 :
		case I_PUSH_LITERAL | 12 : case I_PUSH_LITERAL | 13 :
		case I_PUSH_LITERAL | 14 : case I_PUSH_LITERAL | 15 :
			PUSH(fn->literals[op & 15]);
			break ;
		case I_PUSH_AUTO | 0 : case I_PUSH_AUTO | 1 :
		case I_PUSH_AUTO | 2 : case I_PUSH_AUTO | 3 :
		case I_PUSH_AUTO | 4 : case I_PUSH_AUTO | 5 :
		case I_PUSH_AUTO | 6 : case I_PUSH_AUTO | 7 :
		case I_PUSH_AUTO | 8 : case I_PUSH_AUTO | 9 :
		case I_PUSH_AUTO | 10 : case I_PUSH_AUTO | 11 :
		case I_PUSH_AUTO | 12 : case I_PUSH_AUTO | 13 :
		case I_PUSH_AUTO | 14 : // 15 is used for I_BOOL
			arg = local[op & 15];
			if (! arg)
				except("uninitialized variable: " << symstr(fn->locals[op & 15]));
			PUSH(arg);
			break ;
		case I_EQ_AUTO | 0 : case I_EQ_AUTO | 1 :
		case I_EQ_AUTO | 2 : case I_EQ_AUTO | 3 :
		case I_EQ_AUTO | 4 : case I_EQ_AUTO | 5 :
		case I_EQ_AUTO | 6 : case I_EQ_AUTO | 7 :
			local[op & 7] = TOP();
			break ;
		case I_EQ_AUTO_POP | 0 : case I_EQ_AUTO_POP | 1 :
		case I_EQ_AUTO_POP | 2 : case I_EQ_AUTO_POP | 3 :
		case I_EQ_AUTO_POP | 4 : case I_EQ_AUTO_POP | 5 :
		case I_EQ_AUTO_POP | 6 : case I_EQ_AUTO_POP | 7 :
			local[op & 7] = TOP();
			POP();
			break ;
		case I_CALL_GLOBAL | 0 : case I_CALL_GLOBAL | 1 :
		case I_CALL_GLOBAL | 2 : case I_CALL_GLOBAL | 3 :
		case I_CALL_GLOBAL | 4 : case I_CALL_GLOBAL | 5 :
		case I_CALL_GLOBAL | 6 : case I_CALL_GLOBAL | 7 :
			nargs = op & 7;
			CALLX(globals[i = fetch_global()], CALL, nargs, 0, 0, globals(i));
			break ;
		case I_CALL_GLOBAL_POP | 0 : case I_CALL_GLOBAL_POP | 1 :
		case I_CALL_GLOBAL_POP | 2 : case I_CALL_GLOBAL_POP | 3 :
		case I_CALL_GLOBAL_POP | 4 : case I_CALL_GLOBAL_POP | 5 :
		case I_CALL_GLOBAL_POP | 6 : case I_CALL_GLOBAL_POP | 7 :
			nargs = op & 7;
			CALLPOP(globals[fetch_global()], CALL, nargs, 0, 0);
			break ;
		case I_CALL_MEM | 0 : case I_CALL_MEM | 1 :
		case I_CALL_MEM | 2 : case I_CALL_MEM | 3 :
		case I_CALL_MEM | 4 : case I_CALL_MEM | 5 :
		case I_CALL_MEM | 6 : case I_CALL_MEM | 7 :
			nargs = op & 7;
			mem = fetch_member();
			CALLTOP(mem, nargs, 0, 0, mem.str());
			break ;
		case I_CALL_MEM_POP | 0 : case I_CALL_MEM_POP | 1 :
		case I_CALL_MEM_POP | 2 : case I_CALL_MEM_POP | 3 :
		case I_CALL_MEM_POP | 4 : case I_CALL_MEM_POP | 5 :
		case I_CALL_MEM_POP | 6 : case I_CALL_MEM_POP | 7 :
			nargs = op & 7;
			CALLTOPPOP(fetch_member(), nargs, 0, 0);
			break ;
		case I_CALL_MEM_SELF | 0 : case I_CALL_MEM_SELF | 1 :
		case I_CALL_MEM_SELF | 2 : case I_CALL_MEM_SELF | 3 :
		case I_CALL_MEM_SELF | 4 : case I_CALL_MEM_SELF | 5 :
		case I_CALL_MEM_SELF | 6 : case I_CALL_MEM_SELF | 7 :
			nargs = op & 7;
			mem = fetch_member();
			CALLX(self, mem, nargs, 0, 0, mem.str());
			break ;
		case I_CALL_MEM_SELF_POP | 0 : case I_CALL_MEM_SELF_POP | 1 :
		case I_CALL_MEM_SELF_POP | 2 : case I_CALL_MEM_SELF_POP | 3 :
		case I_CALL_MEM_SELF_POP | 4 : case I_CALL_MEM_SELF_POP | 5 :
		case I_CALL_MEM_SELF_POP | 6 : case I_CALL_MEM_SELF_POP | 7 :
			nargs = op & 7;
			CALLPOP(self, fetch_member(), nargs, 0, 0);
			break ;
		case I_PUSH | SUB :		case I_PUSH | SUB_SELF :
		case I_PUSH | LITERAL :	case I_PUSH | AUTO :
		case I_PUSH | MEM :		case I_PUSH | MEM_SELF :
		case I_PUSH | DYNAMIC :	case I_PUSH | GLOBAL :
			PUSH(get(op));
			break ;
		case I_PUSH_VALUE | FALSE :
			PUSH(SuFalse);
			break ;
		case I_PUSH_VALUE | TRUE :
			PUSH(SuTrue);
			break ;
		case I_PUSH_VALUE | EMPTY_STRING :
			PUSH(SuEmptyString);
			break ;
		case I_PUSH_VALUE | MINUS_ONE :
			PUSH(SuMinusOne);
			break ;
		case I_PUSH_VALUE | ZERO :
			PUSH(SuZero);
			break ;
		case I_PUSH_VALUE | ONE :
			PUSH(SuOne);
			break ;
		case I_PUSH_VALUE | SELF :
			PUSH(self);
			break ;
		case I_PUSH_INT :
			PUSH((short) fetch2());
			break ;
		case I_CALL | SUB :
			nargs = fetch1();
			nargnames = fetch1();
			CALLSUB(nargs, nargnames, (ushort*) ip);
			break ;
		case I_CALL | SUB_SELF :
			nargs = fetch1();
			nargnames = fetch1();
			CALLSUBSELF(nargs, nargnames, (ushort*) ip);
			break ;
		case I_CALL | MEM :
			mem = fetch_member();
			nargs = fetch1();
			nargnames = fetch1();
			CALLTOP(mem, nargs, nargnames, (ushort*) ip, mem.str());
			break ;
		case I_CALL | MEM_SELF :
			mem = fetch_member();
			nargs = fetch1();
			nargnames = fetch1();
			CALLX(self, mem, nargs, nargnames, (ushort*) ip, mem.str());
			break ;
		case I_CALL | AUTO :
		case I_CALL | DYNAMIC :
		case I_CALL | GLOBAL :
			arg = get(op);
			nargs = fetch1();
			nargnames = fetch1();
			CALLX(arg, CALL, nargs, nargnames, (ushort*) ip, "");
			break ;
		case I_CALL | LITERAL :	// actually STACK not LITERAL
			nargs = fetch1();
			nargnames = fetch1();
			CALLTOP(CALL, nargs, nargnames, (ushort*) ip, "");
			break ;
		case I_EACH :
			each = fetch1();
			break ;
		case I_BLOCK :
			jump = fetch_jump();
			i = fetch_local(); // first
			nargs = fetch1();
			PUSH(suBlock(tls().proc->fp, ip - fn->code, i, nargs));
			ip += jump - 2;
			break ;
		case I_JUMP | UNCOND :
			jump = fetch_jump();
			ip += jump;
			if (ip > catcher)
				catcher = 0; // break inside try
			break ;
		case I_JUMP | POP_YES :
			jump = fetch_jump();
			if (popbool())
				ip += jump;
			break ;
		case I_JUMP | POP_NO :
			jump = fetch_jump();
			if (! popbool())
				ip += jump;
			break ;
		case I_JUMP | CASE_YES :
			jump = fetch_jump();
			arg = POP();
			if (TOP() == arg)
				{
				ip += jump;
				POP();
				}
			break ;
		case I_JUMP | CASE_NO :
			jump = fetch_jump();
			arg = POP();
			if (TOP() != arg)
				ip += jump;
			else
				POP();
			break ;
		case I_JUMP | ELSE_POP_YES :
			jump = fetch_jump();
			if (topbool())
				ip += jump;
			else
				POP();
			break ;
		case I_JUMP | ELSE_POP_NO :
			jump = fetch_jump();
			if (! topbool())
				ip += jump;
			else
				POP();
			break ;
		case I_BOOL:
			topbool();
			break;
		case I_TRY :
			// save the catcher offset & stack pointer
			jump = fetch_jump();
			catcher = ip + jump;
			catcher_sp = GETSP();
			catcher_x = fetch_literal();
			break ;
		case I_CATCH :
			// clear the catcher offset
			catcher = 0;
			// skip over the catcher
			jump = fetch_jump();
			ip += jump;
			break ;
		case I_THROW :
			{
			static Value block_return("block return");
			arg = POP();
			if (arg == block_return)
				tls().proc->block_return_value = POP();
			if (Except* e = val_cast<Except*>(arg))
				throw *e;
			else
				throw Except(arg.gcstr());
			break ;
			}
		case I_ADDEQ | (SUB << 4) : case I_ADDEQ | (SUB_SELF << 4) :
		case I_ADDEQ | (AUTO << 4) : case I_ADDEQ | (DYNAMIC << 4) :
		case I_ADDEQ | (MEM << 4) : case I_ADDEQ | (MEM_SELF << 4) :
		case I_SUBEQ | (SUB << 4) : case I_SUBEQ | (SUB_SELF << 4) :
		case I_SUBEQ | (AUTO << 4) : case I_SUBEQ | (DYNAMIC << 4) :
		case I_SUBEQ | (MEM << 4) : case I_SUBEQ | (MEM_SELF << 4) :
		case I_CATEQ | (SUB << 4) : case I_CATEQ | (SUB_SELF << 4) :
		case I_CATEQ | (AUTO << 4) : case I_CATEQ | (DYNAMIC << 4) :
		case I_CATEQ | (MEM << 4) : case I_CATEQ | (MEM_SELF << 4) :
		case I_MULEQ | (SUB << 4) : case I_MULEQ | (SUB_SELF << 4) :
		case I_MULEQ | (AUTO << 4) : case I_MULEQ | (DYNAMIC << 4) :
		case I_MULEQ | (MEM << 4) : case I_MULEQ | (MEM_SELF << 4) :
		case I_DIVEQ | (SUB << 4) : case I_DIVEQ | (SUB_SELF << 4) :
		case I_DIVEQ | (AUTO << 4) : case I_DIVEQ | (DYNAMIC << 4) :
		case I_DIVEQ | (MEM << 4) : case I_DIVEQ | (MEM_SELF << 4) :
		case I_MODEQ | (SUB << 4) : case I_MODEQ | (SUB_SELF << 4) :
		case I_MODEQ | (AUTO << 4) : case I_MODEQ | (DYNAMIC << 4) :
		case I_MODEQ | (MEM << 4) : case I_MODEQ | (MEM_SELF << 4) :
		case I_LSHIFTEQ | (SUB << 4) : case I_LSHIFTEQ | (SUB_SELF << 4) :
		case I_LSHIFTEQ | (AUTO << 4) : case I_LSHIFTEQ | (DYNAMIC << 4) :
		case I_LSHIFTEQ | (MEM << 4) : case I_LSHIFTEQ | (MEM_SELF << 4) :
		case I_RSHIFTEQ | (SUB << 4) : case I_RSHIFTEQ | (SUB_SELF << 4) :
		case I_RSHIFTEQ | (AUTO << 4) : case I_RSHIFTEQ | (DYNAMIC << 4) :
		case I_RSHIFTEQ | (MEM << 4) : case I_RSHIFTEQ | (MEM_SELF << 4) :
		case I_BITANDEQ | (SUB << 4) : case I_BITANDEQ | (SUB_SELF << 4) :
		case I_BITANDEQ | (AUTO << 4) : case I_BITANDEQ | (DYNAMIC << 4) :
		case I_BITANDEQ | (MEM << 4) : case I_BITANDEQ | (MEM_SELF << 4) :
		case I_BITOREQ | (SUB << 4) : case I_BITOREQ | (SUB_SELF << 4) :
		case I_BITOREQ | (AUTO << 4) : case I_BITOREQ | (DYNAMIC << 4) :
		case I_BITOREQ | (MEM << 4) : case I_BITOREQ | (MEM_SELF << 4) :
		case I_BITXOREQ | (SUB << 4) : case I_BITXOREQ | (SUB_SELF << 4) :
		case I_BITXOREQ | (AUTO << 4) : case I_BITXOREQ | (DYNAMIC << 4) :
		case I_BITXOREQ | (MEM << 4) : case I_BITXOREQ | (MEM_SELF << 4) :
		case I_EQ | (SUB << 4) : case I_EQ | (SUB_SELF << 4) :
		case I_EQ | (AUTO << 4) : case I_EQ | (DYNAMIC << 4) :
		case I_EQ | (MEM << 4) : case I_EQ | (MEM_SELF << 4) :
		case I_PREINC | (SUB << 4) : case I_PREINC | (SUB_SELF << 4) :
		case I_PREINC | (AUTO << 4) : case I_PREINC | (DYNAMIC << 4) :
		case I_PREINC | (MEM << 4) : case I_PREINC | (MEM_SELF << 4) :
		case I_PREDEC | (SUB << 4) : case I_PREDEC | (SUB_SELF << 4) :
		case I_PREDEC | (AUTO << 4) : case I_PREDEC | (DYNAMIC << 4) :
		case I_PREDEC | (MEM << 4) : case I_PREDEC | (MEM_SELF << 4) :
		case I_POSTINC | (SUB << 4) : case I_POSTINC | (SUB_SELF << 4) :
		case I_POSTINC | (AUTO << 4) : case I_POSTINC | (DYNAMIC << 4) :
		case I_POSTINC | (MEM << 4) : case I_POSTINC | (MEM_SELF << 4) :
		case I_POSTDEC | (SUB << 4) : case I_POSTDEC | (SUB_SELF << 4) :
		case I_POSTDEC | (AUTO << 4) : case I_POSTDEC | (DYNAMIC << 4) :
		case I_POSTDEC | (MEM << 4) : case I_POSTDEC | (MEM_SELF << 4) :
			{
			Value ob;
			Value x;
			Value y;
			Value m;

			switch (op & 0x8f)
				{
			case I_PREINC :
			case I_PREDEC :
			case I_POSTINC :
			case I_POSTDEC :
				break ;
			default :
				y = POP();
				}

			// assignment operators need old value EXCEPT straight assignment
			bool eq = (op & 0x8f) == I_EQ;
			switch ((op - 0x80) >> 4)
				{
			case SUB :
				m = POP();
				ob = POP();
				if (! eq && ! (x = ob.getdata(m)))
					except("uninitialized member: " << m);
				break ;
			case SUB_SELF :
				m = POP();
				ob = self;
				if (! eq && ! (x = self.getdata(m)))
					except("uninitialized member: " << m);
				break ;
			case AUTO :
				i = fetch_local();
				if (! eq && ! (x = local[i]))
					except("uninitialized variable: " << symstr(fn->locals[i]));
				break ;
			case DYNAMIC :
				i = fetch_local();
				if (! eq && ! local[i])
					local[i] = dynamic(fn->locals[i]);
				if (! eq && ! (x = local[i]))
					except("uninitialized variable: " << symstr(fn->locals[i]));
				break ;
			case MEM :
				ob = POP();
				m = fetch_member();
				if (! eq && ! (x = ob.getdata(m)))
					except("uninitialized member: " << m);
				break ;
			case MEM_SELF :
				ob = self;
				m = fetch_member();
				if (! eq && ! (x = self.getdata(m)))
					except("uninitialized member: " << m);
				break ;
			default :
				unreachable();
				}

			Value z;	// the result, differs from x only for post inc/dec
			switch (op & 0x8f)
				{
			case I_ADDEQ :		z = x = x + y; break ;
			case I_SUBEQ :		z = x = x - y; break ;
			case I_CATEQ :		z = x = cat(x, y); break ;
			case I_MULEQ :		z = x = x * y; z = x; break ;
			case I_DIVEQ :		z = x = x / y; z = x; break ;
			case I_MODEQ :		z = x = x.integer() % y.integer(); break ;
			case I_LSHIFTEQ :	z = x = (ulong) x.integer() << y.integer(); break ;
			case I_RSHIFTEQ :	z = x = (ulong) x.integer() >> y.integer(); break ;
			case I_BITANDEQ :	z = x = (ulong) x.integer() & (ulong) y.integer(); break ;
			case I_BITOREQ :	z = x = (ulong) x.integer() | (ulong) y.integer(); break ;
			case I_BITXOREQ :	z = x = (ulong) x.integer() ^ (ulong) y.integer(); break ;
			case I_EQ :			z = x = y; z = x; break ;
			case I_PREINC :		z = x = x + SuOne; break ;
			case I_PREDEC :		z = x = x - SuOne; break ;
			case I_POSTINC :	z = x; x = x + SuOne; break ;
			case I_POSTDEC :	z = x; x = x - SuOne; break ;
			default :
				unreachable();
				}

			switch ((op - 0x80) >> 4)
				{
			case SUB :
			case MEM :
			case SUB_SELF :
			case MEM_SELF :
				ob.putdata(m, x);
				break ;
			case AUTO :
			case DYNAMIC :
				local[i] = x;
				break ;
			default :
				unreachable();
				}

			PUSH(z);
			break ;
			}
		case I_RETURN_NIL :
			PUSH(Value());
			// fall thru
		case I_RETURN :
			goto done;
		case I_UMINUS :
			TOP() = -TOP();
			break ;
		case I_BITNOT :
			TOP() = ~ TOP().integer();
			break ;
		case I_NOT :
			TOP() = topbool() ? SuFalse : SuTrue;
			break ;
		case I_ADD :
			arg = POP();
			TOP() = TOP() + arg;
			break ;
		case I_SUB :
			arg = POP();
			TOP() = TOP() - arg;
			break ;
		case I_CAT :
			arg = POP();
			TOP() = cat(TOP(), arg);
			break ;
		case I_MUL :
			arg = POP();
			TOP() = TOP() * arg;
			break ;
		case I_DIV :
			arg = POP();
			TOP() = TOP() / arg;
			break ;
		case I_MOD :
			arg = POP();
			TOP() = TOP().integer() % arg.integer();
			break ;
		case I_IS :
			arg = POP();
			TOP() = TOP() == arg ? SuTrue : SuFalse;
			break ;
		case I_ISNT :
			arg = POP();
			TOP() = TOP() != arg ? SuTrue : SuFalse;
			break ;
		case I_LT :
			arg = POP();
			TOP() = TOP() < arg ? SuTrue : SuFalse;
			break ;
		case I_LTE :
			arg = POP();
			TOP() = TOP() <= arg ? SuTrue : SuFalse;
			break ;
		case I_GT :
			arg = POP();
			TOP() = TOP() > arg ? SuTrue : SuFalse;
			break ;
		case I_GTE :
			arg = POP();
			TOP() = TOP() >= arg ? SuTrue : SuFalse;
			break ;
		case I_MATCH :
			{
			gcstring sy = POP().gcstr();
			gcstring sx = TOP().gcstr();
			TOP() = rx_match(sx.buf(), sx.size(), 0, rx_compile(sy))
				? SuTrue : SuFalse;
			break ;
			}
		case I_MATCHNOT :
			{
			gcstring sy = POP().gcstr();
			gcstring sx = TOP().gcstr();
			TOP() = rx_match(sx.buf(), sx.size(), 0, rx_compile(sy))
				? SuFalse : SuTrue;
			break ;
			}
		case I_BITAND :
			arg = POP();
			TOP() = (ulong) TOP().integer() & (ulong) arg.integer();
			break ;
		case I_BITOR :
			arg = POP();
			TOP() = (ulong) TOP().integer() | (ulong) arg.integer();
			break ;
		case I_BITXOR :
			arg = POP();
			TOP() = (ulong) TOP().integer() ^ (ulong) arg.integer();
			break ;
		case I_LSHIFT :
			arg = POP();
			TOP() = (ulong) TOP().integer() << arg.integer();
			break ;
		case I_RSHIFT :
			arg = POP();
			TOP() = (ulong) TOP().integer() >> arg.integer();
			break ;
		default :
			error("invalid op code " << hex << (short) op);
			}
		}
	catch (const Except& e)
		{
		if (e.isBlockReturn())
			{
			if (blockframe || e.fp_fn() != fn)
				throw ;
			PUSH(tls().proc->block_return_value);
			tls().proc->block_return_value = Value();
			goto done;
			}
		else if (catcher &&
			catch_match(fn->literals[catcher_x].str(), e.str()))
			{
			// catch
			verify(GETSP() >= catcher_sp);
			SETSP(catcher_sp);
			ip = catcher;
			catcher = 0;
			PUSH(new Except(e));
			each = -1;
			}
		else
			throw ;
		};
done:
	persist_if_block(TOP());
	return POP();
	}

static bool catch_match(const char* s, const char* exception)
	{
	char* q;
	do
		{
		const char* start = s;
		int len = strlen(s);
		if (NULL != (q = (char*) memchr(s, '|', len)))
			{
			len = q - s;
			s = q + 1;
			}
		if (*start == '*')
			{
			--len;
			++start;
			for (const char* e = exception; *e; ++e)
				if (0 == memcmp(e, start, len))
					return true;
			}
		else if (0 == memcmp(exception, start, len))
			return true;
		}
		while (q);
	return false;
	}

inline Value getdata(Value ob, Value m)
	{
	Value x = ob.getdata(m);
	if (! x)
		except("uninitialized member: " << m);
	if (val_cast<SuClass*>(ob))
		if (SuFunction* sufn = val_cast<SuFunction*>(x))
			return new SuMethod(ob, m, sufn);
	return x;
	}

Value Frame::get(uchar op)
	{
	Value ob;
	Value x;
	Value m;
	int i;

	switch (op & 7)
		{
	case SUB :
		m = POP();
		ob = POP();
		x = getdata(ob, m);
		break ;
	case SUB_SELF :
		m = POP();
		x = getdata(self, m);
		break ;
	case LITERAL :
		x = fn->literals[fetch_literal()];
		// bypass check at end since x may be NULL for block return
		return x;
	case AUTO :
		x = local[i = fetch_local()];
		if (! x)
			except("uninitialized variable: " << symstr(fn->locals[i]));
		break ;
	case DYNAMIC :
		i = fetch_local();
		if (! local[i])
			local[i] = dynamic(fn->locals[i]);
		x = local[i];
		if (! x)
			except("uninitialized variable: " << symstr(fn->locals[i]));
		break ;
	case MEM :
		ob = POP();
		m = fetch_member();
		x = getdata(ob, m);
		break ;
	case MEM_SELF :
		m = fetch_member();
		x = getdata(self, m);
		break ;
	case GLOBAL :
		x = globals[i = fetch_global()];
		if (! x)
			except("uninitialized global: " << globals(i));
		break ;
	default :
		unreachable();
		}
	verify(x);
	return x;
	}

Value dynamic(ushort name)
	{
	for (Frame* f = tls().proc->fp; f >= tls().proc->frames; --f)
		{
		if (! f->fn)
			continue ; // skip primitives
		short n = f->fn->nlocals;
		ushort* locals = f->fn->locals;
		for (short i = 0; i < n; ++i)
			if (locals[i] == name && f->local[i])
				return f->local[i];
		}
	return Value();
	}

static void clear_proc(Proc* proc)
	{
	if (proc)
		proc->clear_unused();
	}

// clear_unused() is called at the start of garbage collection
void clear_unused()
	{
	Fibers::foreach_proc(clear_proc);
	}

void Proc::clear_unused()
	{
	// clear unused frames
	Frame* f = fp + 1;
	int n = (char*) (frames + MAXFRAMES) - (char*) f;
	memset(f, 0, n);
	// clear unused stack
	stack.clear_unused();
	}

#include "testing.h"

class test_catch_match : public Tests
	{
	TEST(1, main)
		{
		verify(catch_match("", "exception"));
		verify(catch_match("*", "exception"));
		verify(catch_match("e", "exception"));
		verify(catch_match("*e", "exception"));
		verify(catch_match("*p", "exception"));
		verify(catch_match("exce", "exception"));
		verify(catch_match("exception", "exception"));
		verify(catch_match("*exception", "exception"));
		verify(! catch_match("x", "exception"));
		verify(! catch_match("*y", "exception"));
		verify(! catch_match("ee", "exception"));
		verify(catch_match("a|e", "exception"));
		verify(catch_match("a|*t", "exception"));
		verify(catch_match("abc|exc", "exception"));
		verify(catch_match("abc|*ion", "exception"));
		verify(catch_match("exc|ghi", "exception"));
		verify(catch_match("*cep|ghi", "exception"));
		verify(catch_match("abc|def|exc", "exception"));
		verify(catch_match("abc|def|*ep", "exception"));
		verify(! catch_match("abc|def", "exception"));
		verify(! catch_match("*abc|*def", "exception"));
		}
	};
REGISTER(test_catch_match);
