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

#include "ostreamstr.h"
#include "gc.h"
#include <stdlib.h>
#include <string.h>

class OstreamStrBuf
	{
public:
	OstreamStrBuf(int n) : buf(new(noptrs) char[n]), len(n), used(0)
		{ }
	void add(const void* s, int n);
	char* str()
		{
		if (! buf)
			return "";
		buf[used] = 0; 
		return buf; 
		}
	void clear()
		{ used = 0; }
	int size() const
		{ return used; }
private:
	char* buf;
	int len;
	int used;
	};

void OstreamStrBuf::add(const void* s, int n)
	{
	if (used + n + 1 > len) // allow 1 for nul added by str()
		buf = (char*) GC_realloc(buf, len = 2 * used + n + 1);
	memcpy(buf + used, s, n);
	used += n;
	}

OstreamStr::OstreamStr(int len) 
	: buf(new OstreamStrBuf(len))
	{ }

Ostream& OstreamStr::write(const void* s, int n)
	{
	buf->add(s, n);
	return *this;
	}

char* OstreamStr::str()
	{
	return buf->str();
	}

void OstreamStr::clear()
	{
	buf->clear();
	}

int OstreamStr::size() const
	{
	return buf->size();
	}
