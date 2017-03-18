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

#include "cmpic.h"
#include <ctype.h>

int strcmpic(const char* s, const char* t)
	{
	for (; tolower(*s) == tolower(*t); ++s, ++t)
		if (*s == 0)
			return 0;
	return tolower(*s) - tolower(*t);
	}

int memcmpic(const char* s, const char* t, int n)
	{
	for (int i = 0; i < n; ++i)
		if (tolower(s[i]) != tolower(t[i]))
			return tolower(s[i]) - tolower(t[i]);
	return 0;
	}

#include "testing.h"

class test_cmpic : public Tests
	{
	TEST(0, strcmpic)
		{
		verify(0 == strcmpic("", ""));
		verify(0 == strcmpic("abc", "abc"));
		verify(0 == strcmpic("aBc", "Abc"));
		verify(0 != strcmpic("abc", "abcd"));
		verify(0 != strcmpic("abcd", "abc"));
		verify(0 != strcmpic("abc", ""));
		verify(0 != strcmpic("", "abc"));
		}
	TEST(1, memcmpic)
		{
		verify(0 == memcmpic("", "", 0));
		verify(0 == memcmpic("abc", "abc", 3));
		verify(0 == memcmpic("aBc", "Abc", 3));
		verify(0 != memcmpic("abc", "abcd", 4));
		verify(0 != memcmpic("abcd", "abc", 4));
		verify(0 != memcmpic("abc", "", 1));
		verify(0 != memcmpic("", "abc", 1));
		}
	};
REGISTER(test_cmpic);
