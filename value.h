#ifndef VALUE_H
#define VALUE_H

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

#include "suvalue.h"
#include "sunumber.h"
#include "symbols.h"
#include <malloc.h>
#include <new>
#include <typeinfo>

class gcstring;
class SuObject;
class Ostream;

#define NUM(n) (new (alloca(sizeof (SuNumber))) SuNumber(n))

#define VAL	(is_int() ? NUM(im.n) : p)

#define INTVAL	((short) 0xffff)

class Value
	{
public:
	Value() : p(0)
		{ }
	Value(SuValue* x) : p(x)
		{ }
	Value(int n)
		{
		if (SHRT_MIN <= n && n <= SHRT_MAX)
			{
			im.type = INTVAL;
			im.n = n;
			}
		else
			p = new SuNumber(n);
		}
	Value(const char* s) : p(symbol(s).p)
		{ }
	Value(const gcstring& s) : p(symbol(s).p)
		{ }

	SuValue* ptr() const
		{ return is_int() ? 0 : p; }
	operator void*() const	// to allow if(val)
		{ return p; }
	Value operator-() const; // neg

	unsigned int hash() const
		{ return is_int() ? im.n : p->hashfn(); }
	int integer() const
		{ return is_int() ? im.n : (p ? p->integer() : 0); }
	bool int_if_num(int* pn) const
		{ return is_int() ? (*pn = im.n, true) : VAL->int_if_num(pn); }
	const char* str_if_str() const
		{ return VAL->str_if_str(); }
	SuNumber* number() const
		{ return is_int() ? new SuNumber(im.n) : VAL->number(); }
	SuObject* object() const
		{ return VAL->object(); }
	SuObject* ob_if_ob() const
		{ return is_int() ? 0 : p ? VAL->ob_if_ob() : 0; }
	int symnum() const
		{ return is_int() && im.n > 0 ? im.n : VAL->symnum(); }
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
		{ return VAL->call(self, member, nargs, nargnames, argnames, each); }
	Value getdata(Value m) const
		{ return VAL->getdata(m); }
	void putdata(Value m, Value x)
		{ VAL->putdata(m, x); }
	gcstring gcstr() const;
	char* str() const;		
	size_t packsize() const
		{ return VAL->packsize(); }
	void pack(char* buf) const
		{ VAL->pack(buf); }
	const char* type() const
		{ return is_int() ? "Number" : p ? VAL->type() : "null"; }
	Named* get_named()
		{ return is_int() || ! p ? 0 : VAL->get_named(); }

	friend bool operator==(Value x, Value y);
	friend bool operator<(Value x, Value y);
	friend Value operator+(Value x, Value y);
	friend Value operator-(Value x, Value y);
	friend Value operator*(Value x, Value y);
	friend Value operator/(Value x, Value y);
	friend Ostream& operator<<(Ostream& os, Value x);

	bool is_int() const
		{ return im.type == INTVAL; }

private:
	union
		{
		SuValue* p;
		struct
			{
			short n;
			short type; // NOTE: should be high word
			} im;
		};
	};

template <class T> inline T val_cast(Value v)
	{
	return dynamic_cast<T>(v.ptr());
	}

NORETURN(cantforce(const char* t1, const char* t2));

template <class T> T force(Value x)
	{
	if (T t = val_cast<T>(x))
		return t;
	cantforce(x ? x.type() : "NULL", typeid(T).name());
	}

template <class T> struct HashFn;

template <> struct HashFn<Value>
	{
	unsigned int operator()(Value x)
		{ return x.hash(); }
	};

extern Value NEW;
extern Value CALL;
extern Value CALL_CLASS;
extern Value CALL_INSTANCE;
extern Value INSTANTIATE;
extern Value SuOne;
extern Value SuZero;
extern Value SuMinusOne;
extern Value SuTrue;
extern Value SuFalse;
extern Value SuEmptyString;

#endif
