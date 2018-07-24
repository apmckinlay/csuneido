// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "lisp.h"
#include "testing.h"

TEST(lisp) {
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
	verify(sort(lisp(6, 3)) == lisp(3, 6));
	verify(sort(lisp(7, 2, 0, 4)) == lisp(0, 2, 4, 7));

	verify(Lisp<int>().sort() == Lisp<int>());
	verify(lisp(5).sort() == lisp(5));
	verify(lisp(6, 3).sort() == lisp(3, 6));
	verify(lisp(7, 2, 0, 4).sort() == lisp(0, 2, 4, 7));

	x = lisp(1, 2, 3, 4);
	const Lisp<int>& y = x;
	verify(y.copy() == lisp(1, 2, 3, 4));

	Lisp<int> empty;
	assert_eq(reverse(empty), empty);
	x = lisp(1, 2, 3);
	assert_eq(set_union(x, empty), x);
	assert_eq(set_union(empty, x), x);
	assert_eq(difference(empty, x), empty);
	assert_eq(difference(x, empty), x);

	x = lisp(11, 22, 33);
	assert_eq(x[0], 11);
	assert_eq(x[1], 22);
	assert_eq(x[2], 33);
	xassert(x[3]);
	xassert(x[999]);
}
