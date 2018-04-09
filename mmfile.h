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
#include "mmoffset.h"
// ReSharper disable once CppUnusedIncludeDirective
#include <stddef.h> // for size_t

//#define MM_KEEPADR

const int MM_HEADER = 4;	// size | type
const int MM_TRAILER = 4;	// size ^ adr
const int MM_OVERHEAD = MM_HEADER + MM_TRAILER;
const int MM_ALIGN = 8; // must be a power of 2
// must be big enough to allow space for types
// e.g. align of 8 = 3 bits for type = 8 types
const int MB_PER_CHUNK = 4;
const int MM_MAX_CHUNKS_MAPPED = 1024 / MB_PER_CHUNK;

enum MmCheck { MMOK, MMEOF, MMERR };

class test_mmfile;

class Mmfile
	{
public:
	explicit Mmfile(const char* filename, bool create = false, bool readonly = false);
	~Mmfile();
	Mmoffset alloc(size_t n, char type, bool zero = true);
	void unalloc(size_t n);
	int64 size() const
		{ return file_size; }
	void* adr(Mmoffset offset);
	static char type(void* p);
	static size_t length(void* p);
	MmCheck mmcheck(Mmoffset offset);
	static size_t align(size_t n)
		{ return ((n - 1) | (MM_ALIGN - 1)) + 1; }
	void set_max_chunks_mapped(int n);
	Mmoffset get_file_size();
	void refresh();

	void sync();

	void* first();
	class iterator
		{
	public:
		iterator() : off(0), mmf(0), err(false)
			{ }
		iterator(Mmoffset o, Mmfile* m) : off(o), mmf(m), err(false)
			{ }
		iterator& operator++();
		iterator& operator--();
		void* operator*() const
			{ return mmf->adr(off); }
		bool operator==(const iterator& iter) const
			{ return off == iter.off; }
		char type();
		size_t size();
		bool corrupt() const
			{ return err; }
		Mmoffset offset() const
			{ return off; }
	private:
		Mmoffset off;
		Mmfile* mmf;
		bool err;
		};
	iterator begin();
	iterator end();

private:
	void set_chunk_size(int n) // for tests only
		{ chunk_size = n; }
	void open(const char* filename, bool create, bool ro);
	void map(int chunk);
	void unmap(int chunk);
	void set_file_size(Mmoffset fs);
	int lru_chunk();
	void evict_chunk();
	friend static void test_mmfile_chunks();
	friend static void test_mmfile_unmap();

	enum { MB_MAX_DB = 16 * 1024 }; // 16 gb
	enum { MAX_CHUNKS = MB_MAX_DB / MB_PER_CHUNK };
#ifdef _WIN32
	void* f;
	void* fm[MAX_CHUNKS];
#else
	int fd;
#endif
	int chunk_size;
	Mmoffset file_size;
	char* base[MAX_CHUNKS];
	int hi_chunk;
	int last_alloc;
	long last_used[MAX_CHUNKS];
	long use_t;
	int chunks_mapped;
	int max_chunks_mapped;
#ifdef MM_KEEPADR
	void* unmapped[MAX_CHUNKS];
#endif
	bool readonly;
	};
