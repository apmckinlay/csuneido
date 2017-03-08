#ifndef BITMAP_H
#define BITMAP_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2003 Suneido Software Corp.
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

typedef unsigned int word;

class Bitmap
	{
public:
	Bitmap(int n, void* p = 0);
	static int required(int n)
		{ return (n / NBITS + 1) * sizeof (word); }
	void clear(int x = 0);

	void set1(int i)
		{ data[i >> SHIFT] |= 1 << (i & MASK); }
	void slowset1(int from, int to);
	void set1(int from, int to);

	void set0(int i)
		{ data[i >> SHIFT] &= ~(1 << (i & MASK)); }
	void slowset0(int from, int to);
	void set0(int from, int to);

	int get(int i)
		{ return 1 & (data[i >> SHIFT] >> (i & MASK)); }

	// post: returns a position > from
	int slownext1(int i);
	int next1(int fromto);
	int slownext0(int i);
	int next0(int from);

	// post: returns a position <= p
	int prev1(int i);

	// NOTE: limit affects clear/next/prev
	void set_limit(int n);

	enum { SHIFT = 5, NBITS = 1 << SHIFT, MASK = NBITS -1 };
	friend class test_bitmap;
private:
	word* data;
	int size;
	int limit;
	};

#endif
