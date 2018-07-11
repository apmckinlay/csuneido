// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

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

