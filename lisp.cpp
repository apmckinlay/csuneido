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

#include "lisp.h"
#include "testing.h"

class test_lisp : public Tests
	{
	TEST(1, lisp)
		{
		Lisp<int> x;

		x = cons(1);
		x = cons(2, x);
		x = cons(3, x);
		verify(car(x) == 3);
		verify(cdr(x) == lisp(2, 1));
		verify(reverse(x) == lisp(1, 2, 3));
		x = lisp(1, 2, 3);
		verify(x.size() == 3);
		verify(x[0] == 1);
		verify(lisp(1, 2, 3) == lisp(1, 2, 3));
		verify(member(x, 2));
		verify(!member(x, 4));
		verify(concat(lisp(1, 2), lisp(3, 4)) == lisp(1, 2, 3, 4));
		verify(lispset(lisp(1, 2, 3, 2)) == lisp(1, 2, 3));
		verify(intersect(lisp(1, 2, 3), lisp(2, 3, 4)) == lisp(2, 3));
		verify(set_union(lisp(1, 2, 3), lisp(2, 3, 4)) == lisp(1, 2, 3, 4));

		verify(sort(Lisp<int>()) == Lisp<int>());
		verify(sort(lisp(5)) == lisp(5));
		verify(sort(lisp(6,3)) == lisp(3,6));
		verify(sort(lisp(7, 2, 0, 4)) == lisp(0, 2, 4, 7));

		verify(Lisp<int>().sort() == Lisp<int>());
		verify(lisp(5).sort() == lisp(5));
		verify(lisp(6,3).sort() == lisp(3,6));
		verify(lisp(7,2,0,4).sort() == lisp(0,2,4,7));

		x = lisp(1, 2, 3, 4);
		const Lisp<int>& y = x;
		verify(y.copy() == lisp(1, 2, 3, 4));

		Lisp<int> empty;
		asserteq(reverse(empty), empty);
		x = lisp(1, 2, 3);
		asserteq(set_union(x, empty), x);
		asserteq(set_union(empty, x), x);
		asserteq(difference(empty, x), empty);
		asserteq(difference(x, empty), x);

		x = lisp(11, 22, 33);
		asserteq(x[0], 11);
		asserteq(x[1], 22);
		asserteq(x[2], 33);
		xassert(x[3]);
		xassert(x[999]);
		}
	};
REGISTER(test_lisp);
