// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "istreamstr.h"
#include <cstring>

class IstreamStrImp
	{
public:
	IstreamStrImp(const char* b, int n) : buf(b), end(b + n), p(b)
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
	const char* buf;
	const char* end;
	const char* p;
	};

IstreamStr::IstreamStr(const char* s)
	: imp(new IstreamStrImp(s, strlen(s)))
	{ }

IstreamStr::IstreamStr(const char* buf, int n)
	: imp(new IstreamStrImp(buf, n))
	{ }

int IstreamStr::get_()
	{ return imp->get(); }

int IstreamStr::tellg()
	{ return imp->tellg(); }

Istream& IstreamStr::seekg(int pos)
	{ imp->seekg(pos); return *this; }

int IstreamStr::read_(char* buf, int n)
	{ return imp->read(buf, n); }

#include "testing.h"

TEST(istreamstr)
	{
	IstreamStr iss("hello\nworld\n");
	const int bufsize = 20;
	char buf[bufsize];
	iss.getline(buf, bufsize);
	verify(iss);
	verify(0 == strcmp("hello", buf));
	verify('w' == iss.peek());
	verify('w' == iss.get());
	iss.putback('W');
	iss.getline(buf, bufsize);
	verify(iss);
	verify(0 == strcmp("World", buf));
	iss.getline(buf, bufsize);
	verify(iss.eof());
	}
