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

#include "ostreamfile.h"
#include <stdio.h>

class OstreamFileImp
	{
public:
	OstreamFileImp(const char* filename, const char* mode) : f(fopen(filename, mode))
		{ }
	void close() const
		{ if (f) fclose(f); }
	void add(const void* s, int n) const
		{ if (f) fwrite(s, n, 1, f); }
	explicit operator bool() const
		{ return f; }
	void flush() const
		{ if (f) fflush(f); }
private:
	FILE* f;
	};

OstreamFile::OstreamFile(const char* filename, const char* mode) 
	: imp(new OstreamFileImp(filename, mode))
	{ }

OstreamFile::~OstreamFile()
	{ imp->close(); }

Ostream& OstreamFile::write(const void* s, int n)
	{
	imp->add(s, n);
	return *this;
	}

void OstreamFile::flush()
	{
	imp->flush();
	}

OstreamFile::operator bool() const
	{
	return bool(*imp);
	}

