/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2003 Suneido Software Corp. 
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

#include "itostr.h"
#include <algorithm>

char* utostr(unsigned int x, char* buf, int radix)
	{
	char* dst = buf;
	do
		*dst++ = "0123456789abcdef"[x % radix];
		while ((x /= radix) > 0);
	*dst = 0;
	std::reverse(buf, dst);
	return buf;
	}

char* itostr(int x, char* buf, int radix)
	{
	char* dst = buf;
	if (x < 0)
		{
		*dst++ = '-';
		x = -x;
		}
	utostr(x, dst, radix);
	return buf;
	}

char* u64tostr(uint64 x, char* buf, int radix)
	{
	char* dst = buf;
	do
		*dst++ = "0123456789abcdef"[x % radix];
		while ((x /= radix) > 0);
	*dst = 0;
	std::reverse(buf, dst);
	return buf;
	}

char* i64tostr(int64 x, char* buf, int radix)
	{
	char* dst = buf;
	if (x < 0)
		{
		*dst++ = '-';
		x = -x;
		}
	u64tostr(x, dst, radix);
	return buf;
	}

#include "testing.h"
	
class test_itostr : public Tests
	{
	TEST(0, utostr)
		{
		char buf[20];
		verify(0 == strcmp(utostr(0, buf), "0"));
		verify(0 == strcmp(utostr(1, buf), "1"));
		verify(0 == strcmp(utostr(1234, buf), "1234"));
		verify(0 == strcmp(utostr(1024, buf, 16), "400"));
		verify(0 == strcmp(utostr(7, buf, 2), "111"));
		}
	TEST(1, itostr)
		{
		char buf[20];
		verify(0 == strcmp(itostr(0, buf), "0"));
		verify(0 == strcmp(itostr(1, buf), "1"));
		verify(0 == strcmp(itostr(1234, buf), "1234"));
		verify(0 == strcmp(itostr(-1, buf), "-1"));
		verify(0 == strcmp(itostr(-1234, buf), "-1234"));
		}
	};
REGISTER(test_itostr);
