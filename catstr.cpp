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
