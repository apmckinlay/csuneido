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

#include "mmfile.h"
#include "ostreamfile.h"
#include "except.h"
#include "fatal.h"
#include "minmax.h"
#include <memory.h>
#include <algorithm>

const char magic[] = { 'S', 'n', 'd', 'o' };
const int FILESIZE_OFFSET = 4;
const int MM_FILEHDR = 8; // should be multiple of align

Mmoffset Mmfile::get_file_size()
	{
	return static_cast<Mmoffset32*>(adr(FILESIZE_OFFSET))->unpack();
	}

inline void Mmfile::set_file_size(Mmoffset fs)
	{
	verify((fs % MM_ALIGN) == 0);
	*(static_cast<Mmoffset32*>(adr(FILESIZE_OFFSET))) = fs;
	}

Mmfile::Mmfile(char* filename, bool create, bool ro)
	: chunk_size(MB_PER_CHUNK * 1024 * 1024), hi_chunk(0),
	hand(0), chunks_mapped(0), max_chunks_mapped(MM_MAX_CHUNKS_MAPPED),
	readonly(ro)
	{
	verify((1 << MM_SHIFT) < MM_ALIGN);
	std::fill(base, base + MAX_CHUNKS, (char*) 0);
#ifdef MM_KEEPADR
	std::fill(unmapped, unmapped + MAX_CHUNKS, (void*) 0);
#endif
	open(filename, create, readonly);
	verify(file_size >= 0);
	verify(file_size < (int64) MB_MAX_DB * 1024 * 1024);
	verify((file_size % MM_ALIGN) == 0);
	if (file_size == 0)
		{
		set_file_size(file_size = MM_FILEHDR);
		memcpy(adr(0), magic, sizeof magic);
		}
	else
		{
		if (0 != memcmp(adr(0), magic, sizeof magic))
			except("not a valid database file (or old version)");
		int64 saved_size = get_file_size();
		// in case the file wasn't truncated last time
		file_size = min(file_size, saved_size);
		}
	}

void Mmfile::refresh()
	{
	file_size = get_file_size();
	}

void Mmfile::set_max_chunks_mapped(int n)
	{
	verify(n >= 1);
	max_chunks_mapped = n;
	}

// blocks have a 4 byte header of size | type
// and a 4 byte trailer

inline size_t len(void* p)
	{ return p ? ((size_t*) p)[-1] & ~(MM_ALIGN - 1) : 0; }

inline char typ(void* p)
	{ return p ? ((size_t*) p)[-1] & (MM_ALIGN - 1) : 0; }

Mmoffset Mmfile::alloc(size_t n, char t, bool zero)
	{
	verify(n < chunk_size);
	last_alloc = n;
	n = align(n);

	// if insufficient room in this chunk, advance to next
	// (by alloc'ing remainder) 
	int chunk = file_size / chunk_size;
	int remaining = (chunk + 1) * chunk_size - file_size;
int r = chunk_size - (file_size % chunk_size);
verify(remaining == r);
	verify(remaining >= MM_OVERHEAD);
	if (remaining < n + MM_OVERHEAD)
		{
		verify(t != 0); // type 0 is filler, filler should always fit
		alloc(remaining - MM_OVERHEAD, 0);
		verify(file_size / chunk_size == chunk + 1);
		}
	verify(t < MM_ALIGN);
	Mmoffset offset = file_size + MM_HEADER;
	file_size += n + MM_OVERHEAD;
	set_file_size(file_size);	// record new file size
	void* p = adr(offset);
	if (zero)
		memset((char*) p - MM_HEADER, 0, n + MM_OVERHEAD);	// zero block
	// header
	((size_t*) p)[-1] = n | t;
	// trailer
	void* q = (char*) p + n;
	*((size_t*) q) = n ^ (offset + n);
	// verify(mmcheck(offset) == MMOK);
	return offset;
	}

// WARNING: use with caution - n MUST be the the value passed to the last alloc
void Mmfile::unalloc(size_t n)
	{
	verify(n == last_alloc);
	n = align(n);
	file_size -= n + MM_OVERHEAD;
	set_file_size(file_size);	// record new file size
	}

void* Mmfile::adr(Mmoffset offset)
	{
	verify(offset >= 0);
	verify(offset < file_size);
	unsigned int chunk = offset / chunk_size;
	verify(chunk < MAX_CHUNKS);
	if (! base[chunk])
		{
		if (chunks_mapped >= max_chunks_mapped)
			evict_chunk();
		map(chunk);
		++chunks_mapped;
		if (chunk > hi_chunk)
			hi_chunk = chunk;
		}
	recently_used[chunk] = true;
	return base[chunk] + (offset % chunk_size);
	}

char Mmfile::type(void* p)
	{
	return typ(p);
	}

size_t Mmfile::length(void* p)
	{
	return len(p);
	}

void* Mmfile::first()
	{
	return file_size <= MM_FILEHDR ? 0 : adr(MM_FILEHDR + MM_HEADER);
	}

Mmfile::iterator Mmfile::begin()
	{
	return iterator(MM_FILEHDR + MM_HEADER, this);
	}

Mmfile::iterator Mmfile::end()
	{
	return iterator(file_size + MM_HEADER, this);
	}

MmCheck Mmfile::mmcheck(Mmoffset o)
	{
	if (o >= file_size + MM_HEADER)
		return MMEOF;
	void* p = adr(o);
	size_t n = len(p);
	if (n > MB_PER_CHUNK * 1024 * 1024) // shouldn't this be chunk_size ?
		return MMERR;
// TODO: check if off + n is in different chunk
	void* q = (char*) p + n;
	if (o + n + sizeof (size_t) > file_size ||
		*((size_t*) q) != (n ^ (unsigned) (o + n)))
		return MMERR;
	return MMOK;
	}

Mmfile::iterator& Mmfile::iterator::operator++()
	{
	verify(off);
	do
		{
		void* p = mmf->adr(off);
		off = off + len(p) + MM_OVERHEAD;
		switch (mmf->mmcheck(off))
			{
		case MMOK :
			break ;
		case MMERR :
			err = true;
			// fall thru
		case MMEOF :
			off = mmf->end().off; // eof or bad block
			return *this;
			}
		} while (type() == 0); // skip filler blocks
	return *this; 
	}

Mmfile::iterator& Mmfile::iterator::operator--()
	{
	verify(off);
	do
		{
		off -= MM_OVERHEAD;
		if (off < mmf->begin().off)
			{
			off = mmf->begin().off;
			return *this;
			}
		void* p = mmf->adr(off);
		size_t n = *((size_t*) p) ^ off;
// TODO: check if off - n is in different chunk
		if (n > MB_PER_CHUNK * 1024 * 1024 || n > off) // shouldn't this be chunk_size ?
			{
			err = true;
			off = mmf->begin().off;
			return *this;
			}
		off -= n;
		switch (mmf->mmcheck(off))
			{
		case MMOK :
			break ;
		case MMERR :
			err = true;
			// fall thru
		case MMEOF :
			off = mmf->begin().off; // eof or bad block
			return *this;
			}
		} while (type() == 0); // skip filler blocks
	return *this; 
	}

char Mmfile::iterator::type()
	{
	return typ(mmf->adr(off));
	}

size_t Mmfile::iterator::size()
	{
	return len(mmf->adr(off));
	}

void mmdump()
	{
	Mmfile m("suneido.db", 0);
	OstreamFile out("mmdump.txt");
	for (Mmfile::iterator iter = m.begin(); iter != m.end(); ++iter)
		{
		out << m.type(*iter) << " " << m.length(*iter) << endl;
		}
	}

void Mmfile::evict_chunk()
	{
#ifndef NDEBUG
	int n = 0;
	for (int i = 0; i < MAX_CHUNKS; ++i)
	if (base[i])
		++n;
	asserteq(n, chunks_mapped);
#endif
	unmap(lru_chunk());
	--chunks_mapped;
	}

int Mmfile::lru_chunk()
	{
	verify(chunks_mapped > 0);
	// the *2 is to allow for two passes
	// first to clear recently_used, second to find it
	for (int i = 0; i < 2 * MAX_CHUNKS; ++i)
		{
		hand = (hand + 1) % MAX_CHUNKS;
		if (! base[hand])
			continue ;
		if (! recently_used[hand])
			return hand;
		recently_used[hand] = false;
		}
	unreachable();
	}

#include "testing.h"
#include <stdio.h> // for remove
#include <string.h>

void add(Mmfile& m, char* s)
	{ strcpy(static_cast<char*>(m.adr(m.alloc(strlen(s) + 1, 1))), s); }

class test_mmfile : public Tests
	{
	TEST(1, main)
		{
		remove("testmm");
		{ Mmfile m("testmm", true);
		//~ for (int i = 0; i < 1100; ++i)
			//~ m.alloc(4000000, 1, false);
		Mmfile::iterator begin = m.end();

		static char* data[] =
			{ "andrew", "leeann", "ken sparrow", "tracy" };
		const int ndata = sizeof data / sizeof (char*);
		
		int i;
		for (i = 0; i < ndata; ++i)
			add(m, data[i]);

		char* p = (char*) *begin;
		for (i = 0; i < ndata; ++i)
			{
			verify(0 == strcmp(p, data[i]));
			p = p + m.length(p) + MM_OVERHEAD;
			}

		Mmfile::iterator iter;
		for (i = 0, iter = begin; iter != m.end(); ++iter, ++i)
			verify(0 == strcmp((char*) *iter, data[i]));
		asserteq(i, ndata);
		
		for (i = ndata - 1, iter = m.end(); iter != begin; --i)
			verify(0 == strcmp((char*) *--iter, data[i]));
		asserteq(i, -1);

		} verify(0 == remove("testmm"));
		}
	TEST(2, chunks)
		{
		const int chunk_size = 65536;
		remove("testmm");
		{ Mmfile m("testmm", true);
		m.set_chunk_size(chunk_size);

		Mmoffset o = m.alloc(65000, 1);
		verify(o < chunk_size);
		o = m.alloc(5000, 1);
		verify(o > chunk_size);

		int n;
		Mmfile::iterator iter;
		for (n = 0, iter = m.begin(); iter != m.end(); ++iter)
			++n;
		asserteq(n, 2);
		
		for (n = 0, iter = m.end(); iter != m.begin(); --iter)
			++n;
		asserteq(n, 2);

		} verify(0 == remove("testmm"));
		}
	TEST(3, lru)
		{
		remove("testmm");
		{ Mmfile m("testmm", true);
		m.unmap(0);
		m.base[111] = (char*) 1;
		m.recently_used[111] = true;
		m.base[222] = (char*) 1;
		m.recently_used[222] = false;
		asserteq(m.lru_chunk(), 222);
		m.base[222] = 0;
		asserteq(m.lru_chunk(), 111);
		m.recently_used[111] = true;
		asserteq(m.lru_chunk(), 111);
		} verify(0 == remove("testmm"));
		}
	TEST(4, unmap)
		{
		remove("testmm");
		{ Mmfile m("testmm", true);
		const int CHUNK_SIZE = 65536;
		m.set_chunk_size(CHUNK_SIZE);
		m.set_max_chunks_mapped(2);
		const int N = CHUNK_SIZE - 500;
		
		// NOTE: chunk 0 is referenced by every alloc via set_filesize
		
		void* p = m.adr(m.alloc(N, 1));
		memset(p, 'a', N);
		verify(m.base[0]); verify(! m.base[1]); verify(! m.base[2]);
		
		p = m.adr(m.alloc(N, 1));
		memset(p, 'b', N);
		verify(m.base[0]); verify(m.base[1]); verify(! m.base[2]);
		
		p = m.adr(m.alloc(N, 1));
		memset(p, 'c', N);
		verify(m.base[0]); verify(! m.base[1]); verify(m.base[2]);
		
		char* s = (char*) m.adr(100);
		verify(*s == 'a');
		verify(m.base[0]); verify(! m.base[1]); verify(m.base[2]);
		
		s = (char*) m.adr(CHUNK_SIZE + 100);
		verify(*s == 'b');
		verify(m.base[0]); verify(m.base[1]); verify(! m.base[2]);
		
		s = (char*) m.adr(2 * CHUNK_SIZE + 100);
		verify(*s == 'c');
		verify(! m.base[0]); verify(m.base[1]); verify(m.base[2]);
		
		} verify(0 == remove("testmm"));
		}
	};
REGISTER(test_mmfile);
