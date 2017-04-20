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

typedef unsigned int size_t;

extern "C"
{
void GC_init();
void* GC_malloc(size_t n);
void* GC_malloc_atomic(size_t n);
void* GC_realloc(void* p, size_t n);
size_t GC_size(void* p);
size_t GC_get_heap_size();
void* GC_base(void* p);
void GC_dump();
void GC_gcollect();

typedef void(*GC_finalization_proc)(void* p, void* data);
void GC_register_finalizer_ignore_self(void* p, GC_finalization_proc fn, void* data,
	GC_finalization_proc* oldfn, void* olddata);
};

inline bool gc_inheap(const void* p)
	{
	return 0 != GC_base(const_cast<void*>(p));
	}

struct NoPtrs { };
extern NoPtrs noptrs;

void* operator new(size_t n, NoPtrs);
void* operator new[](size_t n, NoPtrs);
