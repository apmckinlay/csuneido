#ifndef TEMPDEST_H
#define TEMPDEST_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2002 Suneido Software Corp. 
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

#include <deque>
#include "mmoffset.h"
#include "heap.h"

struct TempNode;

// WARNING: assumes it's safe to convert between pointers and integers
class TempDest
	{
public:
	TempDest();
	void* allocadr(int n);
	Mmoffset alloc(int n)
		{ return off(allocadr(n)); }
	void free();
	static void* adr(Mmoffset offset)
		{ return reinterpret_cast<void*>(offset); }
	static Mmoffset off(void* adr)
		{ return reinterpret_cast<Mmoffset>(adr); }
	void addref(void* p);
	~TempDest();
private:
	Heap heap;
	std::deque<void*> refs;
	int inuse;
	};

#endif
