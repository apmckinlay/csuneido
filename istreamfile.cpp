// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "istreamfile.h"
#include <stdio.h>

class IstreamFileImp
	{
public:
	explicit IstreamFileImp(const char* filename, const char* mode = "r")
		: f(fopen(filename, mode))
		{ }
	void close()
		{
		if (f)
			{
			fclose(f);
			f = nullptr;
			}
		}
	int get() const
		{ return f ? fgetc(f) : -1; }
	int tellg() const
		{ return ftell(f); }
	void seekg(int pos) const
		{ fseek(f, pos, SEEK_SET); }
	int read(char* buf, int n) const
		{ return fread(buf, 1, n, f); }
	int size() const
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

IstreamFile::IstreamFile(const char* filename, const char* mode)
	: imp(new IstreamFileImp(filename, mode))
	{ }

IstreamFile::~IstreamFile()
	{ imp->close(); }

int IstreamFile::get_()
	{
	return imp->get();
	}

int IstreamFile::tellg()
	{ return imp->tellg(); }

Istream& IstreamFile::seekg(int pos)
	{ imp->seekg(pos); return *this; }

int IstreamFile::read_(char* buf, int n)
	{ return imp->read(buf, n); }

int IstreamFile::size() const
	{ return imp->size(); }

void IstreamFile::close() const
	{ imp->close(); }


#include "testing.h"

TEST(istreamfile)
	{
	FILE* f = fopen("test.tmp", "w");
	fputs("hello\n", f);
	fputs("world\n", f);
	fclose(f);

	{ IstreamFile isf("test.tmp");
	const int bufsize = 20;
	char buf[bufsize];
	isf.getline(buf, bufsize);
	verify(isf);
	verify(0 == strcmp("hello", buf));
	verify('w' == isf.peek());
	verify('w' == isf.get());
	isf.putback('W');
	isf.getline(buf, bufsize);
	verify(isf);
	verify(0 == strcmp("World", buf));
	isf.getline(buf, bufsize);
	verify(isf.eof());
	isf.close();
	verify(! isf);
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
