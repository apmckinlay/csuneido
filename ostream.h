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

#include "std.h"

class Ostream;

// output stream manipulators
template <class T> struct OstreamManip
	{
	OstreamManip(void (*f)(Ostream&, T), T x) : fn(f), arg(x)
		{ }
	void (*fn)(Ostream&, T);
	T arg;
	};

// abstract base class for output streams
class Ostream
	{
public:
	Ostream();
	virtual ~Ostream() = default;
	virtual Ostream& write(const void* buf, int n) = 0;
	virtual void flush()
		{ };
	Ostream& write_padded(const char* buf, int n);

	// inserters
	Ostream& operator<<(char c);
	Ostream& operator<<(unsigned char c);
	Ostream& operator<<(char* s);
	Ostream& operator<<(const char* s);
	Ostream& operator<<(short i);
	Ostream& operator<<(unsigned short i);
	Ostream& operator<<(int i);
	Ostream& operator<<(unsigned int i);
	Ostream& operator<<(long i)
		{ return operator<<(static_cast<int>(i)); }
	Ostream& operator<<(unsigned long i)
		{ return operator<<(static_cast<unsigned int>(i)); }
	Ostream& operator<<(int64_t i);
	Ostream& operator<<(uint64_t i);
	Ostream& operator<<(double d);
	Ostream& operator<<(void* p);
	// for manipulators
	Ostream& operator<<(void (*f)(Ostream&))
		{ f(*this); return *this; }
	template <class T> Ostream& operator<<(OstreamManip<T> m)
		{ m.fn(*this, m.arg); return *this; }

	void base(int n)
		{ d_base = n; }
	int base()
		{ return d_base; }
	void fill(char c)
		{ d_fill = c; }
	char fill()
		{ return d_fill; }
	void width(int n)
		{ d_width = n; }
	int width()
		{ return d_width; }
	enum Adjust { LEFT, RIGHT };
	void adjust(Adjust adjust)
		{ d_adjust = adjust; }
	Adjust adjust()
		{ return d_adjust; }
private:
	int d_base;
	char d_fill;
	int d_width;
	Adjust d_adjust;
	};

// manipulators

// output newline
void endl(Ostream& os);

// output nul (zero byte)
void ends(Ostream& os);

// decimal mode
void dec(Ostream& os);

// hex mode
void hex(Ostream& os);

// left justification
void left(Ostream& os);

// right justification
void right(Ostream& os);

// width for next item
OstreamManip<int> setw(int n);

// fill character
OstreamManip<char> setfill(char c);
