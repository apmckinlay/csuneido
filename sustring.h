#ifndef SUSTRING_H
#define SUSTRING_H

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
#include "suvalue.h"
#include "gcstring.h"
#include "hashmap.h"

class SuNumber;

// string values - wraps gcstring
class SuString : public SuValue
	{
public:
	explicit SuString(const gcstring& t) : s(t)
		{ }
	explicit SuString(const char* t) : s(t)
		{ }
	SuString(const char* p, size_t n) : s(p, n)
		{ }
	explicit SuString(size_t n) : s(n)
		{ }
	// CAUTION: constructor that doesn't allocate
	// WARNING: t[n] must be valid, ie. t must be one bigger than n !!!
	SuString(size_t n, const char* t) : s(n, t)
		{ }

	virtual Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);

	virtual Value getdata(Value);

	virtual int integer() const
		{ return strtoul(s.str(), NULL, 0); }
	virtual const char* str_if_str() const
		{ return str(); }
	virtual SuNumber* number();

	int size() const
		{ return s.size(); }

	virtual size_t packsize() const;
	virtual void pack(char* buf) const;
	static SuString* unpack(const gcstring& s);

	static SuString* literal(const char* s);

	char* buf()
		{ return s.buf(); }
	const char* buf() const
		{ return s.buf(); }

	char* begin()
		{ return s.begin(); }
	const char* begin() const
		{ return s.begin(); }

	char* end()
		{ return s.end(); }
	const char* end() const
		{ return s.end(); }

	char* str()
		{ return s.str(); }
	const char* str() const
		{ return s.str(); }

	virtual gcstring gcstr() const
		{ return s; }

	virtual int symnum() const;

	virtual size_t hashfn()
		{ return hash(s.buf(), s.size()); }
	SuString* substr(size_t i, size_t n)
		{ return new SuString(s.substr(i, n)); }
	bool operator==(const char* t) const
		{ return s == t; }
	void adjust()	 // set length to strlen
		{ s = s.substr(0, s.find('\0')); }
	friend SuString* cat(SuString* left, SuString* right);

	virtual void out(Ostream& out);

	static SuString* empty_string;
	
	virtual int order() const;
	virtual bool lt(const SuValue& x) const;
	virtual bool eq(const SuValue& x) const;

	bool is_identifier() const;

protected:
	gcstring s;

private:
	typedef Value (SuString::*pmfn)(short, short, ushort*, int);
	static HashMap<Value,pmfn> methods;

	Value Alphaq(short nargs, short nargnames, ushort* argnames, int each);
	Value AlphaNumq(short nargs, short nargnames, ushort* argnames, int each);
	Value Asc(short nargs, short nargnames, ushort* argnames, int each);
	Value Compile(short nargs, short nargnames, ushort* argnames, int each);
	Value Contains(short nargs, short nargnames, ushort* argnames, int each);
	Value Detab(short nargs, short nargnames, ushort* argnames, int each);
	Value EndsWith(short nargs, short nargnames, ushort* argnames, int each);
	Value Entab(short nargs, short nargnames, ushort* argnames, int each);
	Value Eval(short nargs, short nargnames, ushort* argnames, int each);
	Value Eval2(short nargs, short nargnames, ushort* argnames, int each);
	Value Extract(short nargs, short nargnames, ushort* argnames, int each);
	Value Find(short nargs, short nargnames, ushort* argnames, int each);
	Value FindLast(short nargs, short nargnames, ushort* argnames, int each);
	Value Find1of(short nargs, short nargnames, ushort* argnames, int each);
	Value FindLast1of(short nargs, short nargnames, ushort* argnames, int each);
	Value Findnot1of(short nargs, short nargnames, ushort* argnames, int each);
	Value FindLastnot1of(short nargs, short nargnames, ushort* argnames, int each);
	Value Iter(short nargs, short nargnames, ushort* argnames, int each);
	Value Lower(short nargs, short nargnames, ushort* argnames, int each);
	Value Lowerq(short nargs, short nargnames, ushort* argnames, int each);
	Value Match(short nargs, short nargnames, ushort* argnames, int each);
	Value Mbstowcs(short nargs, short nargnames, ushort* argnames, int each);
	Value Numberq(short nargs, short nargnames, ushort* argnames, int each);
	Value Numericq(short nargs, short nargnames, ushort* argnames, int each);
	Value Repeat(short nargs, short nargnames, ushort* argnames, int each);
	Value Replace(short nargs, short nargnames, ushort* argnames, int each);
	Value ServerEval(short nargs, short nargnames, ushort* argnames, int each);
	Value Size(short nargs, short nargnames, ushort* argnames, int each);
	Value Split(short nargs, short nargnames, ushort* argnames, int each);
	Value StartsWith(short nargs, short nargnames, ushort* argnames, int each);
	Value Substr(short nargs, short nargnames, ushort* argnames, int each);
	Value Tr(short nargs, short nargnames, ushort* argnames, int each);
	Value Unescape(short nargs, short nargnames, ushort* argnames, int each);
	Value Upper(short nargs, short nargnames, ushort* argnames, int each);
	Value Upperq(short nargs, short nargnames, ushort* argnames, int each);
	Value Wcstombs(short nargs, short nargnames, ushort* argnames, int each);

	bool backquote() const;
	};

class SuBuffer : public SuString
	{
public:
	SuBuffer(size_t n, const gcstring& s);
	};

bool is_identifier(const char* s, int n = -1);

#endif
