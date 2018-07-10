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

#include "pack.h"
#include "gcstring.h"
#include "sustring.h"
#include "suboolean.h"
#include "sunumber.h"
#include "sudate.h"
#include "suobject.h"
#include "surecord.h"
#include "cvt.h"
#include "globals.h"
#include "named.h"

Value unpack(const char* buf, int len)
	{
	return unpack(gcstring::noalloc(buf, len));
	}

Value unpack(const gcstring& s)
	{
	if (s.size() == 0)
		return SuEmptyString;
	switch (s[0])
		{
	case PACK_FALSE :
		return SuFalse;
	case PACK_TRUE :
		return SuTrue;
	case PACK_MINUS :
	case PACK_PLUS :
		return SuNumber::unpack(s);
	case PACK_STRING :
		return SuString::unpack(s);
	case PACK_DATE :
		return SuDate::unpack(s);
	case PACK_OBJECT :
		return SuObject::unpack(s);
	case PACK_RECORD :
		return SuRecord::unpack(s);
	default :
		unreachable();
		}
	}

gcstring unpack_gcstr(const gcstring& s)
	{
	return s[0] == PACK_STRING ? s.substr(1) : unpack(s).gcstr();
	}

// string ===========================================================

int packstrsize(const char* s)
	{
	return strlen(s) + 1;
	}

int packstr(char* buf, const char* s)
	{
	int n = strlen(s) + 1;
	memcpy(buf, s, n);
	return n;
	}

const char* unpackstr(const char*& buf)
	{
	const char* s = buf;
	buf += strlen(s) + 1;
	return s;
	}

// Value ============================================================

int packvalue(char* buf, Value x)
	{
	int n = x.packsize();
	cvt_long(buf, n);
	x.pack(buf + sizeof (long));
	return sizeof (long) + n;
	}

Value unpackvalue(const char*& buf)
	{
	int n;
	n = cvt_long(buf);
	buf += sizeof (long);
	Value x = ::unpack(buf, n);
	buf += n;
	return x;
	}

// long =============================================================

size_t packsize(long n)
	{
	if (n == 0)
		return 1;
	if (n < 0)
		n = -n;
//	while (n % 10000 == 0)
//		n /= 10000;
	if (n < 10000)
		return 4;
	if (n < 100000000)
		return 6;
	return 8;
	}

void packlong(char* buf, long n)
	{
	buf[0] = n < 0 ? PACK_MINUS : PACK_PLUS;
	if (n == 0)
		return ;
	int e = 0;
//	for (; n % 10000 == 0; n /= 10000)
//		++e;
	uint32_t x;
	if (n > 0)
		{
		if (n < 10000)
			{
			buf[1] = char(e + 1 ^ 0x80); // exponent
			buf[2] = n >> 8;
			buf[3] = n;
			}
		else if (n < 100000000)
			{
			buf[1] = char(e + 2 ^ 0x80); // exponent
			x = n / 10000;
			buf[2] = x >> 8;
			buf[3] = x;
			x = n % 10000;
			buf[4] = x >> 8;
			buf[5] = x;
			}
		else
			{
			buf[1] = char(e + 3 ^ 0x80); // exponent
			x = n / 100000000;
			buf[2] = x >> 8;
			buf[3] = x;
			n %= 100000000;
			x = n / 10000;
			buf[4] = x >> 8;
			buf[5] = x;
			x = n % 10000;
			buf[6] = x >> 8;
			buf[7] = x;
			}
		}
	else // negative
		{
		n = -n;
		if (n < 10000)
			{
			buf[1] = ~char(e + 1 ^ 0x80); // exponent
			buf[2] = ~(n >> 8);
			buf[3] = ~n;
			}
		else if (n < 100000000)
			{
			buf[1] = ~char(e + 2 ^ 0x80); // exponent
			x = n / 10000;
			buf[2] = ~(x >> 8);
			buf[3] = ~x;
			x = n % 10000;
			buf[4] = ~(x >> 8);
			buf[5] = ~x;
			}
		else
			{
			buf[1] = ~char(e + 3 ^ 0x80); // exponent
			x = n / 100000000;
			buf[2] = ~(x >> 8);
			buf[3] = ~x;
			n %= 100000000;
			x = n / 10000;
			buf[4] = ~(x >> 8);
			buf[5] = ~x;
			x = n % 10000;
			buf[6] = ~(x >> 8);
			buf[7] = ~x;
			}
		}
	}

// unsigned, min coef
uint64_t unpacklongpart(const uint8_t* buf, int sz)
	{
	uint64_t n = 0;
	if (buf[0] == PACK_PLUS)
		{
		if (sz >= 4)
			n = (buf[2] << 8) | buf[3];
		if (sz >= 6)
			n = 10000 * n + ((buf[4] << 8) | buf[5]);
		if (sz >= 8)
			n = 10000 * n + ((buf[6] << 8) | buf[7]);
		if (sz >= 10)
			n = 10000 * n + ((buf[8] << 8) | buf[9]);
		}
	else // PACK_MINUS
		{
		if (sz >= 4)
			n = (uint16_t) ~((buf[2] << 8) + buf[3]);
		if (sz >= 6)
			n = 10000 * n + (uint16_t) ~((buf[4] << 8) | buf[5]);
		if (sz >= 8)
			n = 10000 * n + (uint16_t) ~((buf[6] << 8) | buf[7]);
		if (sz >= 10)
			n = 10000 * n + (uint16_t) ~((buf[8] << 8) | buf[9]);
		}
	return n;
	}

long unpacklong(const gcstring& s)
	{
	int sz = s.size();
	if (sz <= 2)
		return 0;
	verify(sz == 4 || sz == 6 || sz == 8);
	auto buf = (const uint8_t*) s.ptr();
	verify(buf[0] == PACK_PLUS || buf[0] == PACK_MINUS);
	bool minus = buf[0] == PACK_MINUS;
	int e = uint8_t(buf[1]);
	if (minus)
		e = uint8_t(~e);
	e = int8_t(e ^ 0x80) - (sz - 2) / 2;
	long n = unpacklongpart(buf, sz);
	for (; e > 0; --e)
		n *= 10000;
	return minus ? -n : n;
	}

// test =============================================================

#include "testing.h"
#include <malloc.h>

static void testpack(Value x)
	{
	size_t n = x.packsize();
	char* buf = (char*) _alloca(n + 1);
	buf[n] = '\xc4';
	x.pack(buf);
	verify(buf[n] == '\xc4');
	Value y = ::unpack(buf, n);
	assert_eq(x, y);
	}

TEST(pack_boolean)
	{
	testpack(SuTrue);
	testpack(SuFalse);
	verify(SuBoolean::unpack(gcstring("")) == SuBoolean::f);
	}

TEST(pack_string)
	{
	testpack(SuEmptyString);
	testpack(new SuString("hello\nworld"));
	verify(SuString::unpack(gcstring("")) == SuEmptyString);
	}

TEST(pack_number)
	{
	testpack(new SuNumber(0L));
	testpack(new SuNumber(123));
	testpack(new SuNumber(-123));
	testpack(new SuNumber("123.456"));
	testpack(new SuNumber("1234567890"));
	testpack(new SuNumber("-123.456"));
	testpack(new SuNumber("1.23e45"));
	testpack(new SuNumber("-1.23e-45"));
	verify(SuNumber::unpack(gcstring("")) == &SuNumber::zero);
	}

static void testpack(long x)
	{
	char buf[80];
	packlong(buf, x);
	gcstring s = gcstring::noalloc(buf, packsize(x));
	long y = unpacklong(s);
	assert_eq(x, y);
	Value num = ::unpack(s);
	assert_eq(x, num.integer());
	assert_eq(packsize(x), num.packsize());
	char buf2[80];
	num.pack(buf2);
	verify(0 == memcmp(buf, buf2, packsize(x)));
	}
TEST(pack_long)
	{
	testpack(0);
	testpack(1);
	testpack(-1);
	testpack(1234);
	testpack(-1234);
//	testpack(12'0000);
//	testpack(12'0000'0000);
	testpack(12345678);
	testpack(-12345678);
	testpack(INT_MAX);
	testpack(INT_MIN + 1); // no positive equivalent to INT_MIN
	}

#include "porttest.h"
#include "compile.h"
#include "ostreamstr.h"

PORTTEST(compare_packed)
	{
	int n = args.size();
	for (int i = 0; i < n; ++i)
		{
		Value x = compile(args[i].str());
		gcstring px = x.pack();
		for (int j = i + 1; j < n; ++j)
			{
			Value y = compile(args[j].str());
			gcstring py = y.pack();
			if (!(px < py) || (py < px))
				return OSTR("\n\t" << x << " <=> " << y);
			}
		}
	return nullptr;
	}
