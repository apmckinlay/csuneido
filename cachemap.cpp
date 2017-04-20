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

#include "cachemap.h"

#include "testing.h"
#include "except.h"

class test_cachemap : public Tests
	{
	TEST(0,main)
		{
		CacheMap<4,int,int> cache;
		int i;
		for (i = 0; i < 6; ++i)
			{
			cache.put(i, i * 7);
			int* p = cache.get(i);
			verify(p && *p == i * 7);
			}
		for (i = 0; i < 2; ++i)
			verify(! cache.get(i));
		for (i = 2; i < 6; ++i)
			{
			int* p = cache.get(i);
			verify(p && *p == i * 7);
			}
		verify(! cache.get(6));
		}
	};
REGISTER(test_cachemap);
