// Copyright (c) 2018 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// tests ------------------------------------------------------------

#pragma warning(default : 4018 4267 4310)

#include "htbl.h"

#include <tuple>
#include "testing.h"
#include "ostreamstr.h"
#include "gcstring.h"
#include "hashfn.h"

// for TEST(1, hset1)
struct Foo {
	const char* s;
	int n;
};
template <>
struct HashFn<Foo> {
	size_t operator()(const Foo& k) const {
		return hashfn(k.s);
	}
	size_t operator()(const char* s) const {
		return hashfn(s);
	}
};
template <>
struct KeyEq<Foo> {
	bool operator()(const char* x, const Foo& y) const {
		return 0 == strcmp(x, y.s);
	}
	bool operator()(const Foo& x, const Foo& y) const {
		return 0 == strcmp(x.s, y.s);
	}
};
Ostream& operator<<(Ostream& os, const Foo& foo) {
	os << foo.s << ":" << foo.n;
	return os;
}

// for TEST(2, hset2)
struct Bar {
	const char* s;
	int n;
};
struct BarHash {
	size_t operator()(const char* k) {
		return ::hashfn(k);
	}
	size_t operator()(const Bar& k) {
		return ::hashfn(k.s);
	}
};
struct BarEq {
	bool operator()(const char* x, const Bar& y) {
		return 0 == strcmp(x, y.s);
	}
	bool operator()(const Bar& x, const Bar& y) {
		return 0 == strcmp(x.s, y.s);
	}
};
Ostream& operator<<(Ostream& os, const Bar& x) {
	os << x.s << ":" << x.n;
	return os;
}

template <class T>
static gcstring str(const T& x) {
	OstreamStr os;
	os << x;
	return os.gcstr();
}
TEST(htbl_hset) {
	// NOTE: table size of 61 so probe limit doesn't cause resize
	// assumes that int hash is identity, not mixed
	Hset<int> hset(40);
	const int P = 61;
	assert_eq(0, hset.size());
	std::tuple<int, const char*> add[] = {
		{2, "{2}"}, {P + 2, "{2, 63}"},    // collision
		{P + P + 2, "{2, 63, 124}"},       // collision
		{3, "{2, 63, 124, 3}"},            // after collisions
		{1, "{1, 2, 63, 124, 3}"},         // before
		{P + 1, "{1, 62, 63, 124, 2, 3}"}, // bump past group
	};
	for (auto [n, s] : add) {
		hset.insert(n);
		assert_eq(s, str(hset));
	}
	assert_eq(6, hset.size());
	for (auto [n, _] : add) {
		auto p = hset.find(n);
		verify(p);
		assert_eq(n, *p);
		(void) _;
	}
	std::tuple<int, const char*> del[] = {
		{P + 1, "{1, 63, 124, 2, 3}"},
		{1, "{63, 124, 2, 3}"},
		{3, "{63, 124, 2}"},
		{P + 2, "{124, 2}"},
		{2, "{124}"},
		{P + P + 2, "{}"},
	};
	for (auto [n, s] : del) {
		verify(hset.erase(n));
		assert_eq(s, str(hset));
	}
	assert_eq(0, hset.size());
}

TEST(htbl_hset1) {
	// test get by different type (char*) than key (Foo)
	// using standard hashfn and operator==
	Hset<Foo> hset;
	hset.insert(Foo{"one", 123});
	auto p = hset.find("one");
	verify(p);
	assert_eq(123, p->n);
}

TEST(htbl_hset2) {
	// test get by different type (char*) than key (Bar)
	// using explicit custom HfEq
	Hset<Bar, BarHash, BarEq> hset;
	hset.insert(Bar{"one", 123});
	auto p = hset.find("one");
	verify(p);
	assert_eq(123, p->n);
}

TEST(htbl_hmap) {
	Hmap<int, int> hmap;
	verify(!hmap.find(0));
	for (int i = 0; i < 10; ++i)
		hmap.put(i, 100 + i);
	for (int i = 0; i < 10; ++i) {
		int* pi = hmap.find(i);
		verify(pi);
		assert_eq(100 + i, *pi);
		assert_eq(100 + i, hmap[i]);
	}
	assert_eq(10, hmap.size());
	assert_eq(
		"{0: 100, 1: 101, 2: 102, 3: 103, 4: 104, 5: 105, 6: 106, "
		"7: 107, 8: 108, 9: 109}",
		str(hmap));
	// note: order depends on hashing
}

static int mix(uint32_t n) {
	n = ~n + (n << 15);
	n = n ^ (n >> 12);
	n = n + (n << 2);
	n = n ^ (n >> 4);
	n = n * 2057;
	n = n ^ (n >> 16);
	return n;
}
TEST(htbl_randmap) {
	const int N = 40000;

	Hmap<int, int> tbl;

	for (int i = 0; i < N; ++i) {
		// tbl.put(mix(i), i);
		tbl[mix(i)] = i;
	}
	assert_eq(N, tbl.size());

	for (int i = 0; i < N; ++i) {
		auto p = tbl.find(mix(i));
		verify(p);
		assert_eq(i, *p);
		assert_eq(i, tbl[mix(i)]);
	}

	int n = 0;
	for (auto [k, v] : tbl) {
		assert_eq(k, mix(v));
		++n;
	}
	verify(n == N);

	for (int i = 0; i < N; i += 2)
		verify(tbl.erase(mix(i)));
	assert_eq(N / 2, tbl.size());
	for (int i = 0; i < N; ++i)
		if (i & 1)
			assert_eq(i, tbl[mix(i)]);
		else
			verify(!tbl.find(mix(i)));

	n = 0;
	for (auto [k, v] : tbl) {
		assert_eq(k, mix(v));
		++n;
	}
	verify(n == N / 2);
}

static Hset<int> f() {
	Hset<int> tmp;
	tmp.insert(12);
	return tmp;
}
TEST(htbl_set_move) {
	Hset<int> t;
	t.insert(123);
	auto t2 = t; // sets both to readonly since shared data
	xassert(t.insert(789));
	xassert(t2.insert(789));

	auto t3{f()}; // moved so not readonly
	assert_eq(str(t3), "{12}");
	t3.insert(56);

	Hset<int> t4 = f(); // moved so not readonly
	assert_eq(str(t4), "{12}");
	t4.insert(123);
}

static Hmap<int, int> m() {
	Hmap<int, int> tmp;
	tmp.put(9, 999);
	return tmp;
}
TEST(htbl_map_move) {
	Hmap<int, int> t;
	t.put(1, 111);
	auto t2 = t; // sets both to readonly since shared data
	assert_eq(str(t2), "{1: 111}");
	xassert(t.put(2, 222));
	xassert(t2.put(3, 333));

	// auto t3{ m() }; // moved so not readonly
	// assert_eq(str(t3), "{9: 999}");
	// t3.put(4, 444);

	Hmap<int, int> t4 = m(); // moved so not readonly
	assert_eq(str(t4), "{9: 999}");
	t4.put(5, 555);

	Hmap<int, int> x = t4.copy();
	assert_eq(str(x), str(t4));
	x.put(6, 666);
	assert_eq(x.size(), t4.size() + 1);
}

TEST(htbl_grow_restart) {
	Hset<const char*> t;
	// lots of collisions
	t.insert("WindowBase_border");         // hash % 37 == 24
	t.insert("WindowBase_move_observers"); // hash % 37 == 24
	t.insert("Window_newset");             // hash % 37 == 25
	t.insert("WindowBase_rcmdmap");        // hash % 37 == 25
	t.insert("WindowBase_cmdmap");         // hash % 37 == 25
	t.insert("Window_exitOnClose");        // hash % 37 == 26
	// now make it grow
	t.insert("a");
	t.insert("b");
	t.insert("c");
	t.insert("d");
	t.insert("e");
	t.insert("f");
	t.insert("g");
}

TEST(htbl_empty) {
	Hmap<int, int> hm;
	hm.clear();
	hm.reset();
	(void) hm.copy();
	for (auto [k, v] : hm) {
		(void) k;
		(void) v;
	}
}
