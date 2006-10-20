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

#include "permheap.h"
#include "except.h"
#include "port.h"

PermanentHeap::PermanentHeap(char* n, int size) : name(n)
	{
	start = mem_uncommitted(size);
	next = (char*) start;
	commit_end = next;
	limit = next + size;
	}

// NOTE: PAGESIZE must be a power of 2
const int PAGESIZE = 16 * 1024;
inline int roundup(int n)
	{ return ((n - 1) | (PAGESIZE - 1)) + 1; }
inline void* roundup(void* n)
	{ return (void*)((((int) n - 1) | (PAGESIZE - 1)) + 1); }

void* PermanentHeap::alloc(int size)
	{
	void* p = next;
	next += size;
	if (next > commit_end)
		{
		if (next > limit)
			except("out of space for " << name);
		// commit some more
		int n = (char*) roundup(next) - commit_end;
		mem_commit(commit_end, n);
		commit_end += n;
		}
	return p;
	}

const int DECOMMIT_THRESHOLD = 1024 * 1024; // 1mb

void PermanentHeap::free(void* p)
	{
	verify(start <= p && p <= next);
	next = (char*) p;
	p = roundup(p);
	int n = commit_end - (char*) p;
	if (n > DECOMMIT_THRESHOLD)
		{
		mem_decommit(p, n);
		commit_end -= n;
		}
	}

// test =============================================================

#include "testing.h"
#include "random.h"
#include <memory.h>

class test_permheap : public Tests
	{
	TEST(0, main)
		{
		PermanentHeap ph("test", 1024 * 1024);
		for (int i = 0; i < 100; ++i)
			{
			int n = random(10000);
			void* p = ph.alloc(n);
			memset(p, 0, n);
			}
		}
	TEST(1, free)
		{
		PermanentHeap ph("test", 8 * 1024 * 1024);
		void* p = ph.alloc(8);
		void* q = ph.alloc(2 * 1024 * 1024);
		ph.free(q);
		memset(p, 0, 1);
		}
	};
REGISTER(test_permheap);
