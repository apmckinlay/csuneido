/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2017 Suneido Software Corp.
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

#include "list.h"

#include "testing.h"

class test_list : public Tests
	{
	TEST(1, list)
		{
		List<int> v;
		assert_eq(0, v.size());
		v.add(123).add(456);
		assert_eq(2, v.size());
		assert_eq(123, v[0]);
		assert_eq(456, v[1]);
		xassert(v[3]);

		int total = 0;
		for (auto x : v)
			total += x;
		assert_eq(579, total);

		auto v2 = v; // sets both to readonly since shared data
		xassert(v.add(789));
		xassert(v2.add(789));

		auto v3{ f() }; // moved so not readonly
		v3.add(56);
		assert_eq(3, v3.size());
		assert_eq(56, v3[2]);
		assert_eq(true, v3.has(12));
		assert_eq(true, v3.has(34));
		assert_eq(true, v3.has(56));
		assert_eq(false, v3.has(78));

		auto v4{ v3.copy() }; // moved so not readonly
		v4.add(78);
		assert_eq(4, v4.size());
		}
	static List<int> f()
		{
		List<int> tmp;
		tmp.add(12).add(34);
		return tmp;
		}

	TEST(2, set)
		{
		ListSet<int> set;
		set.add(123).add(456).add(123);
		assert_eq(2, set.size());
		assert_eq(true, set.has(123));
		assert_eq(true, set.has(456));

		auto s2{ set.copy() };
		s2.add(789);
		assert_eq(2, set.size());
		assert_eq(3, s2.size());
		}

	TEST(3, equals)
		{
		List<int> v1;
		assert_eq(v1, v1);
		List<int> v2;
		assert_eq(v1, v2);
		v1.add(12).add(34);
		assert_neq(v1, v2);
		assert_neq(v2, v1);
		v2.add(12);
		assert_neq(v1, v2);
		assert_neq(v2, v1);
		v2.add(34);
		assert_eq(v1, v2);
		assert_eq(v2, v1);
		}
	};
REGISTER(test_list);
