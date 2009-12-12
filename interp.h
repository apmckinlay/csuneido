#ifndef INTERP_H
#define INTERP_H

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
#include "except.h"
#include <new>
#include "trace.h"

class SuValue;
class Func;
class SuFunction;
class BuiltinFunc;

enum { FalseNum = 0, TrueNum, OBJECT };

// used for expression evaluation and function arguments
class Stack
	{
public:
	Stack() :  sp(0)
		{ }
	void push(Value x)
		{
		if (sp >= STACKSIZE - 10)
			except("value stack overflow");
		stack[++sp] = x;
//		labels[sp] = 0;
		}
/*	ushort label()
		{ return labels[sp]; }
	void label(short n)
		{ labels[sp] = n; }
*/	Value& top()
		{
		return stack[sp];
		}
	Value pop()
		{
		verify(sp > 0);
		return stack[sp--];
		}
	Value* getsp()
		{ return stack + sp; }
	void setsp(Value* newsp)
		{
		verify(stack <= newsp && newsp < stack + STACKSIZE);
		sp = newsp - stack;
		}
	void clear_unused()
		{ memset(stack + sp + 1, 0, STACKSIZE - sp + 1); }
private:
	enum { STACKSIZE = 1000 };
	Value stack[STACKSIZE];
//	ushort labels[STACKSIZE];
	int sp;
	};

class SuRecord;

// store SuRecord and member for currently active rule for a Frame
struct Rule
	{
	Rule() : rec(0), mem(0)
		{ }
	SuRecord* rec;
	ushort mem;
	};

// a function or block activation
class Frame
	{
public:
	friend struct Framer;

	Frame() // to allow array
		{ }
	Frame(BuiltinFunc*, Value self);
	Frame(SuFunction*, Value self);
	Frame(Frame* fp, int pc, int first, int nargs, Value self);

	Value run();
//private:
	Func* prim;
	SuFunction* fn;
	Value self;
	uchar* ip;		// instruction pointer
	Value* local;	// base pointer to args and autos
	// the rule member currently being evaluated - used to auto-register
	Rule rule;

	uchar* catcher;
	Value* catcher_sp;
	int catcher_x;

	Frame* blockframe;

	uchar fetch_local()
		{ return fetch1(); }
	uchar fetch_literal()
		{ return fetch1(); }
	short fetch_jump()
		{ return fetch2(); }
	ushort fetch_member()
		{ return fetch2(); }
	ushort fetch_global()
		{ return fetch2(); }
	uchar fetch1()
		{ return *ip++; }
	ushort fetch2()
		{ ushort j = ip[1] * 256 + ip[0]; ip += 2; return j; }
	Value get(uchar);
	Value dynamic(ushort);	// dynamic lookup

	int each;
	};

// a process (Fibers)
struct Proc
	{
	Proc() : fp(frames), super(0), in_handler(false), synchronized(0)
		{ }
	void clear_unused();

	Stack stack;
	enum { MAXFRAMES = 180 }; // to fit Proc in 4 pages
	Frame frames[MAXFRAMES];
	Frame *fp;					// points to current frame
	short super;
	bool in_handler;
	Value block_return_value;
	int synchronized; // normally 0 (meaning allow yield), incremented by Synchronized
	};

extern Proc*& tss_proc(); // current Proc

// push and pop Frame's
struct Framer
	{
	Framer(SuFunction* fn, Value self)
		{
		new(nextfp()) Frame(fn, self);
		}
	Framer(BuiltinFunc* prim, Value self)
		{
		new(nextfp()) Frame(prim, self);
		}
	Framer(Frame* fp, int pc, int first, int nargs, Value self)
		{
		new(nextfp()) Frame(fp, pc, first, nargs, self);
		}
	Frame* nextfp()
		{
		if (tss_proc()->fp >= tss_proc()->frames + Proc::MAXFRAMES - 1)
			except("function call overflow");
		return ++tss_proc()->fp;
		}
	~Framer()
		{
		--tss_proc()->fp;
		}
	};

extern int callnest;

// call a standalone or member function
Value docall(Value x, Value member, short nargs = 0, short nargnames = 0, ushort* argnames = 0, int each = -1);

#define ARG(i) tss_proc()->stack.getsp()[1 - nargs + i]

inline void PUSH(Value x)
	{ tss_proc()->stack.push(x); }
inline Value& TOP()
	{ return tss_proc()->stack.top(); }
inline Value POP()
	{ return tss_proc()->stack.pop(); }
inline Value* GETSP()
	{ return tss_proc()->stack.getsp(); }
inline void SETSP(Value* newsp)
	{ tss_proc()->stack.setsp(newsp); }

extern Value block_return;

struct KeepSp
	{
	KeepSp()
		{ sp = GETSP(); }
	~KeepSp()
		{ SETSP(sp); }
	Value* sp;
	};
#define KEEPSP	KeepSp keep_sp;

// evaluate a string of source code
Value run(const char* source);

NORETURN(unknown_method(const char* type, Value member));

#endif
