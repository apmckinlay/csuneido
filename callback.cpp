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

// NOTE: you can't use incremental linking with this code

#include "callback.h"
#include "std.h"
#include "interp.h"
#include "except.h"
#include "hashmap.h"
#include "sublock.h"
#include "suobject.h"
#include "prim.h"
#include "heap.h"
#include "lisp.h"

#ifdef _MSC_VER
#pragma warning(disable:4309) // truncation of constant value
#endif

long callback(Value fn, Callback* cb, char* src);

static Heap heap(true); // true = executable

#if defined(X_MSC_VER)
__declspec(naked) int master()
	{
	_asm
		{
		lea	EAX,4[ESP]
		push EAX			// src
		push 0x55555555		// cb
		push 0x66666666		// fn
		mov EAX, offset callback
		call EAX			// ensure call uses absolute address (not relative)
		add ESP,12
		ret 0x4444
		}
	}
#else
char master[32] = {
	0x8D, 0x44, 0x24, 0x04,				// lea eax,[esp+4]
	0x50,								// push eax
	0x68, 0x55, 0x55, 0x55, 0x55,		// push 55555555h
	0x68, 0x66, 0x66, 0x66, 0x66,		// push 66666666h
	0xB8, 0x77, 0x77, 0x77, 0x77,		// mov eax, 77777777h
	0xFF, 0xD0,							// call eax
	0x83, 0xC4, 0x0C,					// add esp,0Ch
	0xC2, 0x44, 0x44					// ret 4444h
	};
const int CALL_OFFSET = 16;
#endif

const int LENGTH = 32;
const int CB_OFFSET = 6;
const int FN_OFFSET = 11;
const int ARGS_OFFSET = 26;
/*
  Returns a callable pointer
  The actual code is defined above in master()
  which is copied into a new memory block then
  the appropriate arguments are added to the new block
  and the pointer is returned.
*/
void* make_callback(Value fn, Callback* cb, short nargs)
	{
	verify(nargs < 256);
	char* p = (char*) heap.alloc(LENGTH);
	memset(p, 0, LENGTH);
	memcpy(p, (const void*) &master, LENGTH);
	verify(*((int*) (p + CB_OFFSET)) == 0x55555555);
	verify(*((int*) (p + FN_OFFSET)) == 0x66666666);
	verify(*((short*) (p + ARGS_OFFSET)) == 0x4444);
	*((Value*) (p + FN_OFFSET)) = fn;
	*((Callback**) (p + CB_OFFSET)) = cb;
#ifndef X_MSC_VER
	verify(*((int*) (p + CALL_OFFSET)) == 0x77777777);
	*((void**) (p + CALL_OFFSET)) = (void*) &callback;
#endif
	*((short*) (p + ARGS_OFFSET)) = nargs;
	return p;
	}

struct Cb
	{
	Cb()
		{ }
	Cb(void* f, Callback* c) : fn(f), cb(c)
		{ }
	void* fn;
	Callback* cb;
	};
struct CbHashFn
	{
	size_t operator()(Value x)
		{
		if (SuObject* ob = val_cast<SuObject*>(x))
			return (size_t) ob;
		return x.hash();
		}
	};
struct CbEq
	{
	bool operator()(Value x, Value y)
		{
		if (SuObject* xob = val_cast<SuObject*>(x))
			if (SuObject* yob = val_cast<SuObject*>(y))
				return xob == yob;
		return x == y;
		}
	};
typedef HashMap<Value,Cb,CbHashFn,CbEq> CbHash;
CbHash callbacks;

void Callback::put(char*& dst, char*& dst2, const char* lim2, Value x)
	{
	void* f;
	int n;
	if (x && x.int_if_num(&n))
		f = (void*) n;
	else if (callbacks.find(x))
		except("duplicate callbacks not allowed");
	else
		{
		persist_if_block(x);
		f = make_callback(x, this, TypeMulti::size());
		callbacks[x] = Cb(f, this);
		}
	*((void**) dst) = f;
	dst += sizeof (void*);
	}

static Lisp<void*> to_free;

Value su_ClearCallback()
	{
	Value x = proc->stack.top();
	Cb* cb = callbacks.find(x);
	if (cb)
		to_free.push(cb->fn);
	callbacks.erase(x);
	return cb ? SuTrue : SuFalse;
	}
PRIM(su_ClearCallback, "ClearCallback(value)");

void free_callbacks()
	{
	for (; ! nil(to_free); ++to_free)
		heap.free(*to_free);
	}

// called by the functions created by make_callback
long callback(Value fn, Callback* cb, char* src)
	{
	return cb->callback(fn, src);
	}

long Callback::callback(Value fn, char* src)
	{
	Value* oldsp = proc->stack.getsp();
	try
		{
		try
			{
			// convert & push args
			for (int i = 0; i < nitems; ++i)
				{
				Value x = items[i].type().get(src, Value());
				proc->stack.push(x ? (Value) x : (Value) &SuNumber::zero);
				}
	
			// call function
			Value result = docall(fn, CALL, nitems);
			proc->stack.setsp(oldsp);
			return result.integer();
			}
		catch (const std::exception& e)
			{ except(e.what()); }
#ifndef _MSC_VER
		catch (const Except&)
			{ throw ; }
		// with Visual C++, this catches page faults ???
		catch (...)
			{ except("unknown exception"); }
#endif
		}
	catch (const Except& x)
		{
		extern void handler(const Except& x);
		handler(x);
		proc->stack.setsp(oldsp);
		return 0;
		}
	}

void Callback::out(Ostream& os)
	{
	os << named.name() << " /* callback */";
/*
	for (int i = 0; i < nitems; ++i)
		{
		if (i > 0)
			os << ", ";
		items[i].type().out(os);
		os << ' ' << names(mems[i]);
		}
	os << ')';
*/
	}

#include "suobject.h"

Value su_callbacks()
	{
	SuObject* ob = new SuObject;
	for (CbHash::iterator it = callbacks.begin(); it != callbacks.end(); ++it)
		ob->add(it->key);
	return ob;
	}
PRIM(su_callbacks, "Callbacks()");
