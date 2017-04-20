#pragma once
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

#include "value.h"

// abstract base class for types for dll interface
// needs to be SuValue for putting in globals and for Structures
class Type : public SuValue
	{
public:
	virtual int size() = 0; // in dst
	virtual void put(char*& dst, char*& dst2, const char* lim2, Value x) = 0;
	// WARNING: dst is not checked for overflow - call size first
	virtual Value get(const char*& src, Value x) = 0;
	virtual void getbyref(const char*& src, Value x);
	virtual Value result(long, long);
	};

// bool (true or false) Type
class TypeBool : public Type
	{
public:
	int size() override
		{ return sizeof (int); }
	void put(char*& dst, char*& dst2, const char* lim2, Value x) override;
	Value get(const char*& src, Value x) override;
	Value result(long, long n) override
		{ return n ? SuTrue : SuFalse; }
	void out(Ostream& os) const override;
	};

// integer Types
template <class T> class TypeInt : public Type
	{
public:
	int size() override
		{ return sizeof (T); }
	void put(char*& dst, char*&, const char*, Value x) override
		{
		*((T*) dst) = x ? (T) x.integer() : 0;
		dst += sizeof (T);
		}
	Value get(const char*& src, Value x) override
		{
		T n = *((T*) src);
		if (! x || n != x.integer())
			x = n;
		src += sizeof (T);
		return x;
		}
	void out(Ostream& os) const override;
	Value result(long, long n) override
		{ return (T) n; }
	};

template<> class TypeInt<long long> : public Type
	{
public:
	int size() override
		{ return sizeof (int64); }
	void put(char*& dst, char*&, const char*, Value x) override;
	Value get(const char*& src, Value x) override;
	void out(Ostream& os) const override;
	Value result(long hi, long lo) override
		{
		int64 n = hi;
		return (n << 32) + lo;
		}
	};

// opaque pointer type (Suneido treats it like a number)
class TypeOpaquePointer : public TypeInt<long>
	{
	void out(Ostream& os) const override;
	};

// float

class TypeFloat : public Type
	{
public:
	int size() override
		{ return sizeof (float); }
	void put(char*& dst, char*&, const char*, Value x) override;
	Value get(const char*& src, Value x) override;
	void out(Ostream& os) const override;
	Value result(long hi, long lo) override;
	};

class TypeDouble : public Type
	{
public:
	int size() override
		{ return sizeof (double); }
	void put(char*& dst, char*&, const char*, Value x) override;
	Value get(const char*& src, Value x) override;
	void out(Ostream& os) const override;
	Value result(long hi, long lo) override;
	};

// Windows resources - SuHandle, SuGdiObj
template <class T> class TypeWinRes : public Type
	{
	int size() override
		{ return sizeof (void*); }
	void put(char*& dst, char*& dst2, const char* lim2, Value x) override
		{
		if (T* h = val_cast<T*>(x))
			*((void**) dst) = h->handle();
		else
			*((void**) dst) = x ? (void*) x.integer() : 0;
		dst += sizeof (void*);
		}
	Value get(const char*& src, Value x) override
		{
		void* p = *((void**) src);
		src += sizeof (void*);
		if (T* h = val_cast<T*>(x))
			if (h->handle() == p)
				return x;
		return new T(p);
		}
	void out(Ostream& os) const override;
	Value result(long, long n) override
		{ return new T((void*) n); }
	};

// char* that is NOT nul terminated
class TypeBuffer : public Type
	{
public:
	TypeBuffer(bool i = false) : in(i)
		{ }
	int size() override
		{ return sizeof (char*); }
	void put(char*& dst, char*& dst2, const char* lim2, Value x) override;
	Value get(const char*& src, Value x) override;
	void getbyref(const char*& src, Value x) override
		{ get(src, x); }
	void out(Ostream& os) const override;
protected:
	bool in;
	};

// char* that IS nul terminated
class TypeString : public TypeBuffer
	{
public:
	TypeString(bool i = false) : TypeBuffer(i)
		{ }
	Value get(const char*& src, Value x) override;
	void out(Ostream& os) const override;
	Value result(long, long n) override;
	};

// resource is either string or short
class TypeResource : public Type
	{
public:
	int size() override
		{ return sizeof (char*); }
	void put(char*& dst, char*& dst2, const char* lim2, Value x) override;
	Value get(const char*& src, Value x) override;
	void out(Ostream& os) const override;
private:
	static TypeString tstr;
	static TypeInt<short> tint;
	};

// Structure members and TypeParams & Callback parameters
struct TypeItem
	{
	TypeItem() : n(0), gnum(0), tval(0)
		{ }
	Type& type();
	void out(Ostream& os) const;

	short n;	// 0 for pointer, >0 for normal value, <0 for array
	short gnum;
	Type* tval;
	};

// used by TypeMulti and Structure constructors
template <class T> inline T* dup(T* src, int n)
	{
	T* dst = new T[n]; // TODO alloc uninitialized
	memcpy(dst, src, n * sizeof (T));
	return dst;
	}

// base class for Structure, TypeParams, and Callback
class TypeMulti : public Type
	{
public:
	TypeMulti(TypeItem* is, int nitems);
	int size() override;
	friend class Dll;
protected:
	int nitems;
	TypeItem* items;
	};

// used for Dll parameters
// similar to Structure except that values are on stack and out is different
class TypeParams : public TypeMulti
	{
public:
	TypeParams(TypeItem* it, int n) : TypeMulti(it, n)
		{ }
	void put(char*&, char*&, const char*, Value) override;
	Value get(const char*&, Value) override;
	void putall(char*& dst, char*& dst2, const char* lim2, Value* args);
	void getall(const char*& src, Value* args);
	void out(Ostream& os) const override;
	};
