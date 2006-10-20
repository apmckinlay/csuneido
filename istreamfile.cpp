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

#include "istreamfile.h"
#include <stdio.h>

class IstreamFileImp
	{
public:
	IstreamFileImp(char* filename, char* mode = "r") : f(fopen(filename, mode))
		{ }
	void close()
		{ if (f) fclose(f); }
	int get()
		{ return f ? fgetc(f) : -1; }
	operator void*()
		{ return (void*) f; }
	int tellg()
		{ return ftell(f); }
	void seekg(int pos)
		{ fseek(f, pos, SEEK_SET); }
	int read(char* buf, int n)
		{ return fread(buf, 1, n, f); }
	int size()
		{
		int pos = tellg();
		fseek(f, 0, SEEK_END);
		int n = tellg();
		seekg(pos);
		return n;
		}
private:
	FILE* f;
	};

IstreamFile::IstreamFile(char* filename, char* mode) 
	: imp(new IstreamFileImp(filename, mode))
	{ }

IstreamFile::~IstreamFile()
	{ imp->close(); }

int IstreamFile::get_()
	{ 
	return imp->get();
	}

IstreamFile::operator void*()
	{
	return *imp;
	}

int IstreamFile::tellg()
	{ return imp->tellg(); }

Istream& IstreamFile::seekg(int pos)
	{ imp->seekg(pos); return *this; }

int IstreamFile::read_(char* buf, int n)
	{ return imp->read(buf, n); }

int IstreamFile::size()
	{ return imp->size(); }

#include "testing.h"
#include "except.h"
#include "sustring.h"

class test_istreamfile : public Tests
	{
	TEST(0, main)
		{
		FILE* f = fopen("test.tmp", "w");
		fputs("hello\n", f);
		fputs("world\n", f);
		fclose(f);

		{ IstreamFile isf("test.tmp");
		const int bufsize = 20;
		char buf[bufsize];
		verify(isf.getline(buf, bufsize));
		verify(0 == strcmp("hello", buf));
		verify('w' == isf.peek());
		verify('w' == isf.get());
		isf.putback('W');
		verify(isf.getline(buf, bufsize));
		verify(0 == strcmp("World", buf));
		verify(! isf.getline(buf, bufsize));
		verify(isf.eof());
		}

		{ IstreamFile isf("test.tmp");
		const int bufsize = 20;
		char buf[bufsize];
		isf.read(buf, 6);
		verify(isf.gcount() == 6);
		verify(! isf.eof());
		isf.read(buf, 10);
		verify(isf.gcount() == 6);
		verify(isf.eof());
		}

		verify(0 == remove("test.tmp"));
		}
	};
REGISTER(test_istreamfile);
