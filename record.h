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

#include "gcstring.h"
#include "std.h"
#include "mmoffset.h"

class Value;
class Mmfile;

template <class T> struct RecRep;
struct DbRep;

class Record
	{
public:
	Record() : crep(0)
		{ }
	explicit Record(size_t sz);
	explicit Record(const void* r);
	Record(Mmfile* mmf, Mmoffset offset);
	Record(size_t sz, const void* buf);
	int size() const;
	void addraw(const gcstring& s);
	void addval(Value x);
	void addval(const gcstring& s);
	void addval(const char* s);
	void addval(long x);
	void addnil();
	void addmax();
	void addmmoffset(Mmoffset o)
		{ addval(mmoffset_to_int(o)); }
	gcstring getraw(int i) const;
	Value getval(int i) const;
	gcstring getstr(int i) const;
	long getlong(int i) const;
	Mmoffset getmmoffset(int i) const
		{ return int_to_mmoffset(getlong(i)); }

	bool hasprefix(const Record& r);
	bool prefixgt(const Record& r);

	void reuse(int n);
	char* alloc(size_t n);

	size_t cursize() const;
	size_t bufsize() const;
	void* copyto(void* buf) const; // buf MUST be cursize()
	void moveto(Mmfile* mmf, Mmoffset offset);
	Record dup() const;
	void truncate(int i);
	void* ptr() const;
	Mmoffset off() const;
	friend bool nil(const Record& r);
	bool operator==(const Record& r) const;
	bool operator<(const Record& r) const;

	// used by tempindex and slots
	int to_int() const;
	static Record from_int(int n, Mmfile* mmf);
	Mmoffset to_int64() const;
	static Record from_int64(Mmoffset n, Mmfile* mmf);

	Record to_heap() const;
	static const Record empty;
private:
	void init(size_t sz);
	int avail() const;
	void grow(int need);
	static void copyrec(const Record& src, Record& dst);
	union
		{
		RecRep<uchar>* crep;
		RecRep<ushort>* srep;
		RecRep<ulong>* lrep;
		DbRep* dbrep;
		};
	};

inline bool nil(const Record& r)
	{ return r.crep == 0; }

Ostream& operator<<(Ostream& os, const Record& r);
