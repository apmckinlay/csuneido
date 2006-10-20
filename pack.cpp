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
#include "sufunction.h"
#include "suclass.h"
#include "cvt.h"
#include "globals.h"

Value unpack(const gcstring& s)
	{
	if (s.size() == 0)
		return SuString::empty_string;
	switch (s[0])
		{
	case PACK_FALSE :
		return SuBoolean::f;
	case PACK_TRUE :
		return SuBoolean::t;
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
	case PACK_FUNCTION :
		return SuFunction::unpack(s);
	case PACK_CLASS :
		return SuClass::unpack(s);
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
	Value x = ::unpack(gcstring(n, buf));
	buf += n;
	return x;
	}

// Named ============================================================

int packnamesize(const Named& named)
	{
	if (named.num == 0)
		return 1;
	char* s = named.num < 0x8000 ? globals(named.num) : symstr(named.num);
	return 1 + strlen(s) + 1;
	}

enum { NAME_NONE, NAME_SYMBOL, NAME_GLOBAL };

int packname(char* buf, const Named& named)
	{
	*buf = named.num == 0 ? NAME_NONE :
		named.num < 0x8000 ? NAME_GLOBAL : NAME_SYMBOL;
	if (named.num == 0)
		return 1;
	char* s = named.num < 0x8000 ? globals(named.num) : symstr(named.num);
	return 1 + packstr(buf + 1, s);
	}

int unpackname(const char* buf, Named& named)
	{
	if (*buf == 0)
		return 1;
	named.num = *buf == NAME_GLOBAL ? globals(buf + 1) : ::symnum(buf + 1);
	return 1 + strlen(buf + 1) + 1;
	}

// long =============================================================

size_t packsize(long n)
	{
	if (n == 0)
		return 1;
	if (n < 0)
		n = -n;
	if (n < 10000)
		return 4;
	else if (n < 100000000)
		return 6;
	else
		return 8;
	}

// WARNING: doesn't handle 0x80000000

void packlong(char* buf, long n)
	{
	buf[0] = n < 0 ? PACK_MINUS : PACK_PLUS;
	if (n == 0)
		return ;
	ulong x;
	if (n > 0)
		{
		if (n < 10000)
			{
			buf[1] = (char) (1 ^ 0x80); // exponent
			buf[2] = n >> 8;
			buf[3] = n;
			}
		else if (n < 100000000)
			{
			buf[1] = (char) (2 ^ 0x80); // exponent
			x = n / 10000;
			buf[2] = x >> 8;
			buf[3] = x;
			x = n % 10000;
			buf[4] = x >> 8;
			buf[5] = x;
			}
		else
			{
			buf[1] = (char) (3 ^ 0x80); // exponent
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
			buf[1] = ~(char) (1 ^ 0x80); // exponent
			buf[2] = ~(n >> 8);
			buf[3] = ~n;
			}
		else if (n < 100000000)
			{
			buf[1] = ~(char) (2 ^ 0x80); // exponent
			x = n / 10000;
			buf[2] = ~(x >> 8);
			buf[3] = ~x;
			x = n % 10000;
			buf[4] = ~(x >> 8);
			buf[5] = ~x;
			}
		else
			{
			buf[1] = ~(char) (3 ^ 0x80); // exponent
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

long unpacklong(const gcstring& s)
	{
	int sz = s.size();
	if (sz <= 2)
		return 0;
	const uchar* buf = (const uchar*) s.buf();
	verify(buf[0] == PACK_PLUS || buf[0] == PACK_MINUS);
	verify(sz == 2 || sz == 4 || sz == 6 || sz == 8);
	int n = 0;
	if (s[0] == PACK_PLUS)
		{
		if (sz >= 4)
			n = (buf[2] << 8) | buf[3];
		if (sz >= 6)
			n = 10000 * n + ((buf[4] << 8) | buf[5]);
		if (sz >= 8)
			n = 10000 * n + ((buf[6] << 8) | buf[7]);
		}
	else // PACK_MINUS
		{
		if (sz >= 4)
			n = (ushort) ~((buf[2] << 8) + buf[3]);
		if (sz >= 6)
			n = 10000 * n + (ushort) ~((buf[4] << 8) | buf[5]);
		if (sz >= 8)
			n = 10000 * n + (ushort) ~((buf[6] << 8) | buf[7]);
		n = -n;
		}
	return n;
	}

// test =============================================================

#include "testing.h"
#include <malloc.h>

class test_pack : public Tests
	{
	TEST(0, boolean_pack)
		{
		testpack(SuBoolean::t);
		testpack(SuBoolean::f);
		verify(SuBoolean::unpack(gcstring("")) == SuBoolean::f);
		}
	TEST(1, string_pack)
		{
		testpack(new SuString(""));
		testpack(new SuString("hello\nworld"));
		verify(SuString::unpack(gcstring("")) == SuString::empty_string);
		}
	TEST(2, number_pack)
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
	void testpack(Value x)
		{
		size_t n = x.packsize();
		char* buf = (char*) alloca(n + 1);
		buf[n] = '\xc4';
		x.pack(buf);
		verify(buf[n] == '\xc4');
		Value y = ::unpack(gcstring(n, buf)); // no alloc
		asserteq(x, y);
		}
	TEST(3, long_pack)
		{
		testpack(0);
		testpack(1);
		testpack(-1);
		testpack(1234);
		testpack(-1234);
		testpack(12345678);
		testpack(-12345678);
		testpack(LONG_MAX);
		testpack(LONG_MIN + 1); // no positive equivalent to LONG_MIN
		}
	void testpack(long x)
		{
		char buf[80];
		packlong(buf, x);
		gcstring s(packsize(x), buf); // no alloc
		long y = unpacklong(s);
		asserteq(x, y);
		Value num = ::unpack(s);
		asserteq(x, num.integer());
		asserteq(packsize(x), num.packsize());
		char buf2[80];
		num.pack(buf2);
		verify(0 == memcmp(buf, buf2, packsize(x)));
		}
	};
REGISTER(test_pack);
