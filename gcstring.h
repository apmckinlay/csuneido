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

#include <string.h>
#include "hashfn.h"

class Ostream;

// immutable string class for use with garbage collection
// defers concatenation by making a linked list of pieces
class gcstring
	{
	// invariant: if n is 0 then p is empty_buf
public:
	gcstring() : n(0), p(empty_buf)
		{ }
	explicit gcstring(size_t nn);
	gcstring(const gcstring& s) = default;
	gcstring& operator=(const gcstring& s) = default;

	gcstring(const char* s)
		{ init(s, s ? strlen(s) : 0); }
	gcstring(const char* s, size_t len)
		{ init(s, len); }
	gcstring& operator=(const char* s)
		{
		init(s, strlen(s));
		return *this;
		}

	// CAUTION: p must be one bigger than n to allow for nul
	// use salloc() to handle this
	static gcstring noalloc(const char* p, size_t n)
		{ return gcstring(n, p); }
	static gcstring noalloc(const char* s)
		{ return gcstring(strlen(s), s); }

	size_t size() const
		{ return n >= 0 ? n : -n; }

	typedef const char* const_iterator;
	const_iterator begin() const
		{ ckflat(); return p; }
	const_iterator end() const
		{ ckflat(); return p + n; }

	const char* ptr() const // not necessarily nul terminated
		{ ckflat(); return p; }
	const char* str() const; // nul terminated

	const char& operator[](int i) const
		{ ckflat(); return p[i]; }

	gcstring& operator+=(const gcstring& s);
	gcstring& operator+=(const char* s)
		{ return *this += gcstring(strlen(s), s); }

	int find(char c, size_t pos = 0) const;
	int find(const gcstring& x, size_t pos = 0) const;
	int findlast(const gcstring& x, size_t pos = 0) const;
	int find(const char* s, size_t pos = 0) const
		{ return find(gcstring(strlen(s), s), pos); }
	bool has(const char* s) const
		{ return -1 != find(s); }
	bool has(const gcstring& s) const
		{ return -1 != find(s); }

	gcstring substr(size_t i, int len = -1) const;
	gcstring trim() const;

	bool has_prefix(const gcstring& s, size_t pos = 0) const;
	bool has_suffix(const gcstring& s) const;

	gcstring to_heap();

	gcstring capitalize() const;
	gcstring uncapitalize() const;

private:
	mutable int n; // mutable because of flatten, negative means concat
	union
		{
		mutable const char* p; // mutable because of str()
		struct Concat* cc;
		};
	static const char* empty_buf;

	// CAUTION: constructor that doesn't allocate
	// WARNING: p2[n2] must be valid, ie. p2 must be one bigger than n2 !!!
	// p2[n2] should be nul if you will use str()
	gcstring(size_t n2, const char* p2)
		: n(n2), p(n2 == 0 ? empty_buf : p2)
		{ }
	friend class SuString;

	void init(const char* p2, size_t n2);

	void ckflat() const
		{ if (n < 0) flatten(); }
	void flatten() const;
	static void copy(char* s, const gcstring* p);
	friend class SuBuffer;
	char* buf()
		{ ckflat(); return (char*)p; }
	};

inline bool operator==(const gcstring& x, const gcstring& y)
	{ return x.size() == y.size() && 0 == memcmp(x.ptr(), y.ptr(), x.size()); }
inline bool operator==(const gcstring& x, const char* y)
	{ return x == gcstring::noalloc(y); }
inline bool operator==(const char* x, const gcstring& y)
	{ return gcstring::noalloc(x) == y; }

inline bool operator!=(const gcstring& x, const char* y)
	{ return ! (x == y); }
inline bool operator!=(const char* x, const gcstring& y)
	{ return ! (x == y); }
inline bool operator!=(const gcstring& x, const gcstring& y)
	{ return ! (x == y); }

inline gcstring operator+(const char* s, const gcstring& t)
	{
	return gcstring::noalloc(s) += t;
	}
inline gcstring operator+(const gcstring& s, const char* t)
	{
	gcstring x(s);
	return x += gcstring::noalloc(t);
	}
inline gcstring operator+(const gcstring& s, const gcstring& t)
	{
	gcstring x(s);
	return x += t;
	}

bool operator<(const gcstring& x, const gcstring& y);
inline bool operator<(const gcstring& x, const char* y)
	{ return x < gcstring::noalloc(y); }
inline bool operator<(const char* x, const gcstring& y)
	{ return gcstring::noalloc(x) < y; }

Ostream& operator<<(Ostream& os, const gcstring& s);

template <class T> struct HashFn;

template <> struct HashFn<gcstring>
	{
	size_t operator()(const gcstring& s) const
		{ return hashfn(s.ptr(), s.size()); }
	};

bool has_prefix(const char* s, const char* pre);
bool has_suffix(const char* s, const char* pre);

char* salloc(int n);
