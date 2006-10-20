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

#include "tempdest.h"
#include "except.h"
#include <algorithm>
#include "errlog.h"
#include "port.h"
#include "gc.h"

int tempdest_inuse = 0;

TempDest::TempDest() : inuse(0)
	{
	}

void TempDest::addref(void* p)
	{ 
	if (gc_inheap(p))
		refs.push_back(p);
	}

void* TempDest::allocadr(int n)
	{
	++tempdest_inuse;
	++inuse;
	return static_cast<TempNode*>(heap.alloc(n));
	}

void TempDest::free()
	{
	heap.destroy();
	tempdest_inuse -= inuse;
	inuse = 0;
	std::fill(refs.begin(), refs.end(), (void*) 0); // to help gc
	refs.clear();
	}

TempDest::~TempDest()
	{
	free();
	}

#include "testing.h"

class test_tempdest : public Tests
	{
	TEST(0, main)
		{
		int orig_alloced = tempdest_inuse;
		TempDest td;
		td.allocadr(4000);
		td.allocadr(4000);
		td.free();
		verify(tempdest_inuse == orig_alloced);
		}
	TEST(1, capacity)
		{
		TempDest td;
		for (int i = 0; i < 100 * 1024; ++i)
			td.alloc(4096);
		}
	};
REGISTER(test_tempdest);
