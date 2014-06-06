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

#include "type.h"
#include "suboolean.h"
#include "sustring.h"
#include "suobject.h"
#include "except.h"
#include "globals.h"
#include "minmax.h"

#define check2(n)	if ((dst2) + (n) > lim2) except("conversion overflow")

Value Type::result(long, long)
	{ error("not a valid return type"); }

void Type::getbyref(char*& src, Value x)
	{
	src += size();
	}

//===================================================================

void TypeBool::put(char*& dst, char*& dst2, const char* lim2, Value x)
	{
	if (x)
		force<SuBoolean*>(x);
	*((int*) dst) = (x == SuTrue ? 1 : 0);
	dst += sizeof (int);
	}

Value TypeBool::get(char*& src, Value x)
	{
	x = *((int*) src) ? SuTrue : SuFalse;
	src += sizeof (int);
	return x;
	}

void TypeBool::out(Ostream& os)
	{ os << "bool"; }

//===================================================================

template<>
void TypeInt<char>::out(Ostream& os)
	{ os << "char"; }

template<>
void TypeInt<short>::out(Ostream& os)
	{ os << "short"; }

template<>
void TypeInt<long>::out(Ostream& os)
	{ os << "long"; }

void TypeInt<int64>::out(Ostream& os)
	{ os << "int64"; }

void TypeInt<int64>::put(char*& dst, char*&, const char*, Value x)
	{
	int64 n;
	if (! x)
		n = 0;
	else if (x.is_int())
		n = x.integer();
	else if (SuNumber* num = val_cast<SuNumber*>(x))
		n = num->bigint();
	else
		except("can't convert " << x.type() << " to integer");
	*((int64*) dst) = n;
	dst += sizeof (int64);
	}

Value TypeInt<int64>::get(char*& src, Value x)
	{
	int64 n = *((int64*) src);
	src += sizeof (int64);
	if (SuNumber* num = val_cast<SuNumber*>(x))
		if (n == num->bigint())
			return x;
	return SuNumber::from_int64(n);
	}

//===================================================================

void TypeOpaquePointer::out(Ostream& os)
	{ os << "pointer"; }

//===================================================================

void TypeFloat::put(char*& dst, char*&, const char*, Value x)
	{
	*((float*) dst) = x ? (float) x.number()->to_double() : 0;
	dst += sizeof (float);
	}

Value TypeFloat::get(char*& src, Value x)
	{
	float n = *((float*) src);
	src += sizeof (float);
	return SuNumber::from_float(n);
	}

void TypeFloat::out(Ostream& os)
	{ os << "float"; }

Value TypeFloat::result(long, long)
	{
	float n;
#if defined(_MSC_VER)
	_asm fstp dword ptr [n]
#elif defined(__GNUC__)
    asm("fstp %0" : "=m"(n));
#else
#warning "replacement for inline assembler required"
#endif
	return SuNumber::from_float(n);
	}

void TypeDouble::put(char*& dst, char*&, const char*, Value x)
	{
	*((double*) dst) = x ? (double) x.number()->to_double() : 0;
	dst += sizeof (double);
	}

Value TypeDouble::get(char*& src, Value x)
	{
	double n = *((double*) src);
	src += sizeof (double);
	return SuNumber::from_double(n);
	}

void TypeDouble::out(Ostream& os)
	{ os << "double"; }

Value TypeDouble::result(long, long)
	{
	double n;
#if defined(_MSC_VER)
	_asm fstp qword ptr [n]
#elif defined(__GNUC__)
    asm("fstp %0" : "=m"(n));
#else
#warning "replacement for inline assembler required"
#endif
	return SuNumber::from_double(n);
	}

//===================================================================

template<>
void TypeWinRes<SuHandle>::out(Ostream& os)
	{ os << "handle"; }

template<>
void TypeWinRes<SuGdiObj>::out(Ostream& os)
	{ os << "gdiobj"; }

//===================================================================

// Implements pointer-to-type: don't confuse with TypeOpaquePointer
class TypePointer : public Type
	{
public:
	int size()
		{ return sizeof (void*); }
	void put(char*& dst, char*& dst2, const char* lim2, Value x);
	Value get(char*& src, Value x);
	void getbyref(char*& src, Value x);
	void out(Ostream& os)
		{ os << type << '*'; }
	Value result(long, long n);
	Type* type;
	};

void TypePointer::put(char*& dst, char*& dst2, const char* lim2, Value x)
	{
	if (! x || x == SuZero)
		{
		*((void**) dst) = 0;
		dst += sizeof (void*);
		}
	else
		{
		*((void**) dst) = dst2;
		dst += sizeof (void*);
		int n = type->size();
		check2(n);
		char* dst3 = dst2 + n;
		type->put(dst2, dst3, lim2, x);
		dst2 = dst3;
		}
	}

Value TypePointer::get(char*& src, Value x)
	{
	char* src2 = *((char**) src);
	x = src2 ? type->get(src2, x) : Value();
	src += sizeof (void*);
	return x;
	}

void TypePointer::getbyref(char*& src, Value x)
	{
	if (char* src2 = *((char**) src))
		type->getbyref(src2, x);
	src += sizeof (void*);
	}

Value TypePointer::result(long, long n)
	{
	char* src = (char*) n;
	return type->get(src, 0);
	}

//===================================================================

void TypeBuffer::put(char*& dst, char*& dst2, const char* lim2, Value x)
	{
	SuString* str;
	if (! x || x == SuZero)
		{
		// missing members
		*((char**) dst) = 0;
		}
	else if (in && (str = val_cast<SuString*>(x)))
		{
		// pass pointer to actual string
		*((char**) dst) = str->str();
		}
	else if (SuBuffer* buf = val_cast<SuBuffer*>(x))
		{
		// pass pointer to actual buffer
		*((char**) dst) = buf->buf();
		}
	else
		{
		// copy string to dst2
		gcstring s = x.gcstr();
		*((char**) dst) = dst2;
		int n = s.size();
		check2(n + 1);
		memcpy(dst2, s.buf(), n);
		dst2 += n;
		*dst2++ = 0; // ensure nul terminated
		}
	dst += sizeof (char*);
	}

Value TypeBuffer::get(char*& src, Value x)
	{
	char* now = *((char**) src);
	if (! now)
		x = SuBoolean::f;
	else if (! x)
		x = new SuString(now);
	else if (SuBuffer* buf = val_cast<SuBuffer*>(x))
		verify(now == buf->buf());
	else
		{
		gcstring s = x.gcstr();
		if (0 != memcmp(now, s.buf(), s.size()))
			x = new SuString(now);
		}
	src += sizeof (char*);
	return x;
	}

void TypeBuffer::out(Ostream& os)
	{ 
	os << "buffer";
	}

// TypeString inherits from TypeBuffer ==============================

Value TypeString::get(char*& src, Value x)
	{
	if (in)
		{
		src += sizeof (char*);
		return x;
		}
	x = TypeBuffer::get(src, x);
	if (SuBuffer* buf = val_cast<SuBuffer*>(x))
		buf->adjust();
	return x;
	}

Value TypeString::result(long, long n)
	{
	char* s = (char*) n;
	if (s)
		return new SuString(s);
	else
		return SuBoolean::f;
	}

void TypeString::out(Ostream& os)
	{
	if (in)
		os << "[in] ";
	os << "string"; 
	}

//===================================================================

inline int short_int(Value x)
	{
	int i;
	return x.int_if_num(&i) && SHRT_MIN <= i && i <= SHRT_MAX;
	}

void TypeResource::put(char*& dst, char*& dst2, const char* lim2, Value x)
	{
	if (short_int(x))
		tint.put(dst, dst2, lim2, x);
	else
		tstr.put(dst, dst2, lim2, x);
	}

Value TypeResource::get(char*& src, Value x)
	{
	char* save_src = src;
	x = tint.get(src, x);
	if (short_int(x))
		return x;
	src = save_src;
	return tstr.get(src, x);
	}

void TypeResource::out(Ostream& os)
	{ 
	os << "resource"; 
	}

TypeString TypeResource::tstr;
TypeInt<long> TypeResource::tint;

//===================================================================

class TypeArray : public Type
	{
public:
	explicit TypeArray(int size) : n(size)
		{ }
	int size();
	void put(char*& dst, char*& dst2, const char* lim2, Value x);
	Value get(char*& src, Value x);
	void getbyref(char*& src, Value x)
		{ get(src, x); }
	void out(Ostream& os)
		{ os << type << '[' << n << ']'; }
	Type* type;
	int n;
	};

int TypeArray::size()
	{
	if (dynamic_cast<TypeBuffer*>(type))
		return n;
	return n * type->size();
	}

void TypeArray::put(char*& dst, char*& dst2, const char* lim2, Value x)
	{
	if (dynamic_cast<TypeBuffer*>(type)) // includes TypeString
		{
		if (! x)
			*dst = 0;
		else if (SuString* sx = val_cast<SuString*>(x)) // includes SuBuffer
			{
			// string[] is special case for strings
			int len = min(sx->size(), n);
			memcpy(dst, sx->buf(), len);
			if (dynamic_cast<TypeString*>(type))
				dst[min(sx->size(),n-1)] = 0;
			}
		dst += n;
		return ;
		}	
	if (! x)
		x = new SuObject;//(n);
	SuObject* ob = x.object();
	for (int i = 0; i < n; ++i)
		type->put(dst, dst2, lim2, ob->get(i));
	}

Value TypeArray::get(char*& src, Value x)
	{
	if (dynamic_cast<TypeString*>(type))
		{
		// string[] is special case for strings
		char* s = src;
		src += n;
		SuString* sx = val_cast<SuString*>(x);
		int len = strlen(s);
		if (sx && len == sx->size() && 0 == memcmp(s, sx->buf(), len))
			return x;
		return new SuString(s);
		}
	if (dynamic_cast<TypeBuffer*>(type))
		{
		// buffer[] is special case for strings
		char* s = src;
		src += n;
		SuString* sx = val_cast<SuString*>(x);
		if (sx && n == sx->size() && 0 == memcmp(s, sx->buf(), n))
			return x;
		return new SuString(s,n);
		}
	if (! x)
		x = new SuObject;//(n);
	SuObject* ob = x.object();
	if (ob->get_readonly())
		src += size();
	else
		for (int i = 0; i < n; ++i)
			{
			Value old = ob->get(i);
			Value now = type->get(src, old);
			if (old != now)
				ob->put(i, now);
			}
	return x;
	}

//===================================================================

Type& TypeItem::type()
	{
	if (globals[gnum] != (Value) tval)
		{
		if (tval == 0)
			{
			if (n == 0)
				tval = new TypePointer;
			else if (n < 0)
				tval = new TypeArray(-n);
			}
		Type* x = force<Type*>(globals[gnum]);
		if (n > 0) // normal value
			tval = x;
		else if (n == 0) // pointer
			{
			verify(tval);
			((TypePointer*) tval)->type = x;
			}
		else // (n < 0) // array
			{
			verify(tval);
			((TypeArray*) tval)->type = x;
			}
		}
	verify(tval);
	return *tval;
	}

void TypeItem::out(Ostream& os)
	{
	os << globals(gnum);
	if (n == 0)
		os << '*';
	else if (n < 0)
		os << '[' << -n << ']';
	}

//===================================================================

TypeMulti::TypeMulti(TypeItem* it, int n) : nitems(n)
	{
	items = dup(it, n);
	}

int TypeMulti::size()
	{
	int n = 0;
	for (int i = 0; i < nitems; ++i)
		n += items[i].type().size();
	return n;
	}

//===================================================================

void TypeParams::putall(char*& dst, char*& dst2, const char* lim2, Value* args)
	{
	for (int i = 0; i < nitems; ++i)
		items[i].type().put(dst, dst2, lim2, args[i]);
	}

void TypeParams::getall(char*& src, Value* args)
	{
	// NOTE: no need to update args
	for (int i = 0; i < nitems; ++i)
		items[i].type().getbyref(src, args[i]);
	}

// this isnt really necessary since dll and callback add names
void TypeParams::out(Ostream& os)
	{
	os << '(';
	for (int i = 0; i < nitems; ++i)
		{
		if (i > 0)
			os << ", ";
		items[i].out(os);
		}
	os << ')';
	}

void TypeParams::put(char*&, char*&, const char*, Value)
	{ 
	error("should not be used"); 
	}

Value TypeParams::get(char*&, Value)
	{ 
	error("should not be used");
	}

//===================================================================

#include "testing.h"

class test_types : public Tests
	{
	TEST(0, main)
		{
		test1(new TypeBool, sizeof (int), 0, SuTrue);
		test1(new TypeBool, sizeof (int), 0, SuFalse);
		test1(new TypeInt<short>, sizeof (short), 0, 123);
		test1(new TypeInt<short>, sizeof (short), 0, 0);
		test1(new TypeInt<short>, sizeof (short), 0, -123);
		test1(new TypeInt<long>, sizeof (long), 0, 123);
		test1(new TypeInt<long>, sizeof (long), 0, 0);
		test1(new TypeInt<long>, sizeof (long), 0, -123);
		test1(new TypeOpaquePointer, sizeof (long), 0, 123);
		test1(new TypeOpaquePointer, sizeof (long), 0, 0);
		test1(new TypeOpaquePointer, sizeof (long), 0, -123);
		test1(new TypeFloat, sizeof (float), 0, 123);
		test1(new TypeFloat, sizeof (float), 0, new SuNumber("123.456"));
		test1(new TypeDouble, sizeof (double), 0, 123);
		test1(new TypeDouble, sizeof (double), 0, new SuNumber("123.456"));
		}
	void test1(Type* type, int size, int size2, Value x)
		{
		char buf[64];
		char buf2[64];
		char* dst = buf;
		char* dst2 = buf2;
		char* lim2 = buf2 + sizeof buf2;

		asserteq(type->size(), size);
		type->put(dst, dst2, lim2, x);
		verify(dst == buf + size);
		verify(dst2 == buf2 + size2);
		dst = buf;
		asserteq(type->get(dst, Value()), x);
		}
	};
REGISTER(test_types);
