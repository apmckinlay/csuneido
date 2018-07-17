// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "cmpic.h"
#include <cctype>

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

TEST(cmpic_str)
	{
	verify(0 == strcmpic("", ""));
	verify(0 == strcmpic("abc", "abc"));
	verify(0 == strcmpic("aBc", "Abc"));
	verify(0 != strcmpic("abc", "abcd"));
	verify(0 != strcmpic("abcd", "abc"));
	verify(0 != strcmpic("abc", ""));
	verify(0 != strcmpic("", "abc"));
	}

TEST(cmpic_mem)
	{
	verify(0 == memcmpic("", "", 0));
	verify(0 == memcmpic("abc", "abc", 3));
	verify(0 == memcmpic("aBc", "Abc", 3));
	verify(0 != memcmpic("abc", "abcd", 4));
	verify(0 != memcmpic("abcd", "abc", 4));
	verify(0 != memcmpic("abc", "", 1));
	verify(0 != memcmpic("", "abc", 1));
	}
