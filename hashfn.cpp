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

#include "hashfn.h"

// FNV1a see: 
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function

size_t hashfn(const char* s)
	{
	size_t hash = 2166136261;
	for (; *s; ++s)
		{
		hash = hash ^ *s;
		hash = hash * 16777619;
		}
	return hash;
	}

size_t hashfn(const char* s, int n)
	{
	size_t hash = 2166136261;
	for (auto end = s + n; s < end; ++s)
		{
		hash = hash ^ *s;
		hash = hash * 16777619;
		}
	return hash;
	}

#include "testing.h"

class test_hashfn : public Tests
	{
	TEST(0, string)
		{
		auto s = "one";
		auto h = hashfn(s);
		// both are equivalent
		assert_eq(h, hashfn(s, strlen(s)));
		// order dependent
		assert_neq(h, hashfn("eno"));
		}
	};
REGISTER(test_hashfn);
