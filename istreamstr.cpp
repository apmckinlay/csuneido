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

#include "istreamstr.h"
#include <string.h>

class IstreamStrImp
	{
public:
	IstreamStrImp(char* b, int n) : buf(b), end(b + n), p(b)
		{ }
	int get()
		{ return p < end ? *p++ : -1; }
	int tellg() const
		{ return p - buf; }
	void seekg(int pos)
		{ p = buf + pos; }
	int read(char* dst, int n)
		{ if (n > end - p) n = end - p; memcpy(dst, p, n); p += n; return n; }
private:
	char* buf;
	char* end;
	char* p;
	};

IstreamStr::IstreamStr(char* s)
	: imp(new IstreamStrImp(s, strlen(s)))
	{ }

IstreamStr::IstreamStr(char* buf, int n)
	: imp(new IstreamStrImp(buf, n))
	{ }

int IstreamStr::get_()
	{ return imp->get(); }

IstreamStr::operator bool() const
	{ return true; }

int IstreamStr::tellg()
	{ return imp->tellg(); }

Istream& IstreamStr::seekg(int pos)
	{ imp->seekg(pos); return *this; }

int IstreamStr::read_(char* buf, int n)
	{ return imp->read(buf, n); }

#include "testing.h"
#include "except.h"

class test_istreamstr : public Tests
	{
	TEST(0, main)
		{
		IstreamStr iss("hello\nworld\n");
		const int bufsize = 20;
		char buf[bufsize];
		verify(iss.getline(buf, bufsize));
		verify(0 == strcmp("hello", buf));
		verify('w' == iss.peek());
		verify('w' == iss.get());
		iss.putback('W');
		verify(iss.getline(buf, bufsize));
		verify(0 == strcmp("World", buf));
		verify(! iss.getline(buf, bufsize));
		verify(iss.eof());
		}
	};
REGISTER(test_istreamstr);
