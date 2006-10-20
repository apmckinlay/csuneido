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

#include "hashmap.h"
#include "testing.h"

class test_hashmap : public Tests
	{
	TEST(0, main)
		{
		const int N = 1000;

		typedef HashMap<int,int> htbl;
		htbl tbl;

		srand(123);
		int i;
		for (i = 0; i < N; ++i)
			tbl[f(i)] = rand();
		verify(tbl.size() == N);

		srand(123);
		for (i = 0; i < N; ++i)
			verify(tbl[f(i)] == rand());
		int n = 0;
		for (htbl::iterator p = tbl.begin(); p != tbl.end(); ++p, ++n)
			{
			i = ff(p->key);
			verify(0 <= i && i < N);
			}
		verify(n == N);

		for (i = 0; i < N; ++i)
			verify(tbl.erase(f(i)));
		verify(tbl.size() == 0);
		}
	int f(int x) 
		{ return (x + 7654321) ^ 1234567; }
	int ff(int x) 
		{ return (x ^ 1234567) - 7654321; }
	};
REGISTER(test_hashmap);
