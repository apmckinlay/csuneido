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
#include "gcstring.h"
#include "buffer.h"

OstreamStr::OstreamStr(int len) : buf(new Buffer(len))
	{ }

Ostream& OstreamStr::write(const void* s, int n)
	{
	buf->add((const char*) s, n);
	return *this;
	}

char* OstreamStr::str()
	{
	return buf->str();
	}

gcstring OstreamStr::gcstr()
	{
	return buf->gcstr();
	}

void OstreamStr::clear()
	{
	buf->clear();
	}

int OstreamStr::size() const
	{
	return buf->size();
	}
