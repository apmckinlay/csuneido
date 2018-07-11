// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "catstr.h"
#include <stdarg.h>

char *catstr(char* buf, ...)
	{
	va_list srcs;
	char* dst = buf;

	va_start(srcs, buf);
	while (char* src = va_arg(srcs, char*))
		{
		for (; 0 != (*dst = *src); ++src, ++dst)
			{ }
		}
	*dst = 0;
	return buf;
	}

#include "testing.h"
#include <cstring>

TEST(catstr)
	{
	char buf[128];

	memset(buf, '-', 128);
	catstr(buf + 1, "hello", " ", "world", NULL);
	verify(0 == strcmp(buf + 1, "hello world"));
	verify(buf[0] == '-');
	verify(buf[13] == '-');
	}

TEST(catstr_CATSTRA)
	{
	verify(0 == strcmp(CATSTRA("hello", "world"), "helloworld"));
	}

TEST(catstr_STRDUPA)
	{
	verify(0 == strcmp(STRDUPA("hello"), "hello"));
	}

TEST(catstr_PREFIXA)
	{
	verify(0 == strcmp(PREFIXA("helloworld", 5), "hello"));
	}
