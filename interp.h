#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "std.h"
#include "value.h"
#include "except.h"
#include "fibers.h" // for tls().proc
// ReSharper disable once CppUnusedIncludeDirective
#include "noargs.h"

class SuValue;
class Func;
class SuFunction;
class BuiltinFuncs;

// used for expression evaluation and function arguments
class Stack {
public:
	Stack() : sp(0) {
	}
	void push(Value x) {
		if (sp >= STACKSIZE - 10)
			except_log_stack("value stack overflow");
		stack[++sp] = x;
	}
	Value& top() {
		return stack[sp];
	}
	Value pop() {
		verify(sp > 0);
		return stack[sp--];
	}
	Value* getsp() {
		return stack + sp;
	}
	void setsp(Value* newsp) {
		verify(stack <= newsp && newsp < stack + STACKSIZE);
		sp = newsp - stack;
	}
	void clear_unused() {
		memset(stack + sp + 1, 0, STACKSIZE - sp + 1);
	}

private:
	enum { STACKSIZE = 1000 };
	Value stack[STACKSIZE];
	int sp;
};

class SuRecord;

// store SuRecord and member for currently active rule for a Frame
struct Rule {
	SuRecord* rec = nullptr;
	short mem = 0;
};

// a function or block activation
class Frame {
public:
	friend struct Framer;

	Frame() { // to allow array
	}
	Frame(BuiltinFuncs*, Value self);
	Frame(SuFunction*, Value self);
	Frame(Frame* fp, int pc, int first, int nargs, Value self);

	Value run();

	Func* prim = nullptr;
	SuFunction* fn = nullptr;
	Value self;
	uint8_t* ip = nullptr;  // instruction pointer
	Value* local = nullptr; // base pointer to args and autos
	Rule rule; // rule member currently being evaluated - used to auto-register
	bool created_block = false;

private:
	uint8_t fetch_local() {
		return fetch1();
	}
	short fetch_literal();
	short fetch_jump() {
		return fetch2();
	}
	Value fetch_member();
	bool jumpToPopReturn();
	uint16_t fetch_global() {
		return fetch2();
	}
	uint8_t fetch1() {
		return *ip++;
	}
	uint16_t fetch2() {
		uint16_t j = ip[1] * 256 + ip[0];
		ip += 2;
		return j;
	}
	Value get(uint8_t);

	uint8_t* catcher = nullptr;
	Value* catcher_sp = nullptr;
	int catcher_x = 0;

	bool isblockframe = false;

	int each = 0;
};

Value dynamic(short);

enum { NOSUPER = -1 };

// a process (Fibers)
struct Proc {
	void clear_unused();

	Stack stack;
	enum { MAXFRAMES = 256 };
	Frame frames[MAXFRAMES];
	Frame* fp = frames; // points to current frame
	short super = NOSUPER;
	bool in_handler = false;
	Value block_return_value;
};

// push and pop Frame's
struct Framer {
	Framer(SuFunction* fn, Value self) {
		new (nextfp()) Frame(fn, self);
	}
	Framer(BuiltinFuncs* prim, Value self) {
		new (nextfp()) Frame(prim, self);
	}
	Framer(Frame* fp, int pc, int first, int nargs, Value self) {
		new (nextfp()) Frame(fp, pc, first, nargs, self);
	}
	static Frame* nextfp() {
		if (tls().proc->fp >= tls().proc->frames + Proc::MAXFRAMES - 1)
			except_log_stack("function call overflow");
		return ++tls().proc->fp;
	}
	~Framer() {
		--tls().proc->fp;
	}
};

extern int callnest;

// call a standalone or member function
Value docall(Value x, Value member, short nargs = 0, short nargnames = 0,
	short* argnames = nullptr, int each = -1);

#define ARG(i) tls().proc->stack.getsp()[1 - nargs + (i)]

inline void PUSH(Value x) {
	tls().proc->stack.push(x);
}
inline Value& TOP() {
	return tls().proc->stack.top();
}
inline Value POP() {
	return tls().proc->stack.pop();
}
inline Value* GETSP() {
	return tls().proc->stack.getsp();
}
inline void SETSP(Value* newsp) {
	tls().proc->stack.setsp(newsp);
}

// save and restore stack pointer
struct KeepSp {
	KeepSp() {
		sp = GETSP();
	}
	~KeepSp() {
		SETSP(sp);
	}
	Value* sp;
};
#define KEEPSP KeepSp keep_sp;

// evaluate a string of source code
Value run(const char* source);
