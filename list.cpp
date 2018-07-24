// Copyright (c) 2017 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "list.h"

// tests ------------------------------------------------------------

#include "testing.h"

static List<int> f() {
	List<int> tmp;
	tmp.add(12).add(34);
	return tmp;
}
TEST(list) {
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

	auto v3{f()}; // moved so not readonly
	v3.add(56);
	assert_eq(3, v3.size());
	assert_eq(56, v3[2]);
	assert_eq(true, v3.has(12));
	assert_eq(true, v3.has(34));
	assert_eq(true, v3.has(56));
	assert_eq(false, v3.has(78));

	auto v4{v3.copy()}; // moved so not readonly
	v4.add(78);
	assert_eq(4, v4.size());
}

TEST(list_set) {
	ListSet<int> set;
	set.add(123).add(456).add(123);
	assert_eq(2, set.size());
	assert_eq(true, set.has(123));
	assert_eq(true, set.has(456));

	auto s2{set.copy()};
	s2.add(789);
	assert_eq(2, set.size());
	assert_eq(3, s2.size());
}

TEST(list_equals) {
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

TEST(list_erase) {
	List<int> v;
	assert_eq(false, v.erase(56));
	v.add(12).add(34).add(56).add(78);
	assert_eq(true, v.erase(34));
	assert_eq(List<int>({12, 56, 78}), v);
	assert_eq(true, v.erase(78)); // last
	assert_eq(List<int>({12, 56}), v);
	assert_eq(12, v.popfront());
	assert_eq(List<int>({56}), v);
}

TEST(list_overflow) {
	List<int> v;
	for (int i = 0; i < USHRT_MAX - 1; ++i)
		v.add(i);
	xassert(v.add(99999));
}
