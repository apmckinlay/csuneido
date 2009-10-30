#ifndef SUDATE_H
#define SUDATE_H

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

// date and time values
class SuDate : public SuValue
	{
	friend class test_date;
public:
	SuDate();
	SuDate(int d, int t);

	void* operator new(size_t n);
	void* operator new(size_t n, void* p)
		{ return p; }

	static Value suclass(); // singleton as class

	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);

	void out(Ostream& out);

	// database packing
	size_t packsize() const
		{ return 1 + sizeof (int) + sizeof (int); }
	void pack(char* buf) const;
	static SuDate* unpack(const gcstring& s);
	static Value literal(const char* s);

	// for use as subscript
	size_t hashfn()
		{ return date ^ time; }

	int order() const;
	bool lt(const SuValue& x) const;
	bool eq(const SuValue& x) const;
	
	bool operator<(const SuDate& d) const;

	SuDate& increment();
	static SuDate* timestamp();
	static Value parse(char* s, char* order = "yMd");
	static int minus_ms(SuDate* d1, SuDate* d2);
private:
	static Value instantiate(short nargs, short nargnames, ushort* argnames, int each);
	Value FormatEn(short nargs, short nargnames, ushort* argnames, int each);
	Value Plus(short nargs, short nargnames, ushort* argnames, int each);
	Value MinusDays(short nargs, short nargnames, ushort* argnames, int each);
	Value MinusSeconds(short nargs, short nargnames, ushort* argnames, int each);
	Value Year(short nargs, short nargnames, ushort* argnames, int each);
	Value Month(short nargs, short nargnames, ushort* argnames, int each);
	Value Day(short nargs, short nargnames, ushort* argnames, int each);
	Value Hour(short nargs, short nargnames, ushort* argnames, int each);
	Value Minute(short nargs, short nargnames, ushort* argnames, int each);
	Value Second(short nargs, short nargnames, ushort* argnames, int each);
	Value Millisecond(short nargs, short nargnames, ushort* argnames, int each);
	Value WeekDay(short nargs, short nargnames, ushort* argnames, int each);

	int date;
	int time;
	};

#endif
