// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostream.h"
#include "itostr.h"
#include <cstdlib>
#include <cstring>

Ostream::Ostream() : d_base(10), d_fill(' '), d_width(0), d_adjust(RIGHT)
	{ }

// manipulators

void endl(Ostream& os)
	{
	os.write("\n", 1);
	os.flush();
	}

void ends(Ostream& os)
	{
	os.write("\0", 1);
	}

void dec(Ostream& os)
	{
	os.base(10);
	}

void hex(Ostream& os)
	{
	os.base(16);
	}

void left(Ostream& os)
	{
	os.adjust(Ostream::LEFT);
	}

void right(Ostream& os)
	{
	os.adjust(Ostream::RIGHT);
	}

static void setfill_(Ostream& os, char c)
	{ os.fill(c); }
OstreamManip<char> setfill(char c)
	{ return OstreamManip<char>(setfill_, c); }

static void setw_(Ostream& os, int n)
	{ os.width(n); }
OstreamManip<int> setw(int n)
	{ return OstreamManip<int>(setw_, n); }

// inserters

static void pad(Ostream& os, int w)
	{
	char c = os.fill();
	for (int n = os.width() - w; n > 0; --n)
		os.write(&c, 1);
	os.width(0);
	}

Ostream& Ostream::write_padded(const char* buf, int n)
	{
	if (adjust() == Ostream::RIGHT)
		pad(*this, n);
	write(buf, n);
	if (adjust() == Ostream::LEFT)
		pad(*this, n);
	return *this;
	}

Ostream& Ostream::operator<<(char c)
	{
	return write_padded(&c, 1);
	}

Ostream& Ostream::operator<<(unsigned char c)
	{
	return write_padded((char*) &c, 1);
	}

Ostream& Ostream::operator<<(char* s)
	{
	return write_padded(s, strlen(s));
	}

Ostream& Ostream::operator<<(const char* s)
	{
	return write_padded(s, strlen(s));
	}

Ostream& Ostream::operator<<(short i)
	{
	char buf[32];
	itostr(i, buf, base());
	return write_padded(buf, strlen(buf));
	}

Ostream& Ostream::operator<<(unsigned short i)
	{
	char buf[32];
	itostr(i, buf, base());
	return write_padded(buf, strlen(buf));
	}

Ostream& Ostream::operator<<(int i)
	{
	char buf[32];
	itostr(i, buf, base());
	return write_padded(buf, strlen(buf));
	}

Ostream& Ostream::operator<<(unsigned int i)
	{
	char buf[32];
	utostr(i, buf, base());
	return write_padded(buf, strlen(buf));
	}

Ostream& Ostream::operator<<(int64_t i)
	{
	char buf[I64BUF];
	i64tostr(i, buf, base());
	return write_padded(buf, strlen(buf));
	}

Ostream & Ostream::operator<<(uint64_t i)
	{
	char buf[I64BUF];
	u64tostr(i, buf, base());
	return write_padded(buf, strlen(buf));
	}

Ostream& Ostream::operator<<(double d)
	{
	char buf[_CVTBUFSIZE];
	_gcvt(d, 6, buf);
	return write_padded(buf, strlen(buf));
	}

Ostream& Ostream::operator<<(void* p)
	{
	char buf[32];
	strcpy(buf, "0x");
	utostr((intptr_t) p, buf + 2, 16);
	return write_padded(buf, strlen(buf));
	}
