/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2013 Suneido Software Corp. 
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

#include "buffer.h"
#include "gc.h"
#include "except.h"
#include "gcstring.h"
#include "minmax.h"
#include "fatal.h"
#include <string.h>

Buffer::Buffer(int n) : buf(new(noptrs) char[n]), capacity(n), used(0), pos(0)
	{ }

char* Buffer::ensure(int n)
	{
	++n;  // allow for nul for gcstr
	if (used + n > capacity)
		{
		buf = (char*) GC_realloc(buf, capacity = max(2 * capacity, capacity + n));
		if (! buf)
			fatal("out of memory");
		}
	return buf + used;
	}

char* Buffer::alloc(int n)
	{
	char* dst = ensure(n);
	added(n);
	return dst;
	}

Buffer& Buffer::add(char c)
	{
	*alloc(1) = c;
	return *this;
	}

Buffer& Buffer::add(const char* s, int n)
	{
	memcpy(alloc(n), s, n);
	return *this;
	}

Buffer& Buffer::add(const gcstring& s)
	{
	int n = s.size();
	memcpy(alloc(n), s.buf(), n);
	return *this;
	}

void Buffer::remove(int n)
	{
	verify(n <= used);
	memmove(buf, buf + n, used - n);
	used -= n;
	}

gcstring Buffer::gcstr()
	{
	return gcstring(used, str()); // no alloc
	}

char* Buffer::str()
	{
	verify(used < capacity);
	buf[used] = 0;
	return buf;
	}

gcstring Buffer::getStr(int n)
	{
	verify(pos + n <= used);
	gcstring s(buf + pos, n);
	pos += n;
	return s;
	}

char* Buffer::getBuf(int n)
	{
	verify(pos + n <= used);
	int i = pos;
	pos += n;
	return buf + i;
	}
