// Copyright (c) 2005 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "btree.h"

// tests ------------------------------------------------------------

#include "testing.h"
#include <vector>

class TestDest {
public:
	Mmoffset alloc(int n) {
		void* p = new char[n];
		blocks.push_back(p); // protect from garbage collect
		return reinterpret_cast<Mmoffset>(p);
	}
	void free() {
	}
	static void* adr(Mmoffset offset) {
		return reinterpret_cast<void*>(offset);
	}
	static Mmoffset off(void* adr) {
		return reinterpret_cast<Mmoffset>(adr);
	}
	void addref(void* p) {
	}
	std::vector<void*> blocks;
};

typedef Btree<Vslot, VFslot, Vslots, VFslots, TestDest> TestBtree;

#include "value.h"
#include <cmath>

#define assertfeq(x, y) \
	do { \
		float x_ = x; \
		auto y_ = (float) (y); \
		except_if(fabs(x_ - y_) > .05f, \
			__FILE__ << ':' << __LINE__ << ": " \
					 << "error: " << #x << " (" << x_ << ") not close to " \
					 << #y << " (" << y_ << ")"); \
	} while (false)
#define assertclose(x, y) verify(fabs((x) - (y)) < .35)

static Record key(int i) {
	Record r;
	r.addval(Value(i));
	return r;
}
static Record key(const char* s) {
	Record r;
	r.addval(s);
	return r;
}
static Record endkey(int i) {
	Record r = key(i);
	r.addmax();
	return r;
}
static Record maxkey() {
	Record r;
	r.addmax();
	return r;
}
static gcstring make_filler() {
	const int N = 500;
	char* buf = salloc(N);
	memset(buf, ' ', N);
	return gcstring::noalloc(buf, N);
}
static Record bigkey(int i) {
	static gcstring filler = make_filler();
	Record r;
	r.addval(Value(i));
	r.addval(filler);
	return r;
}

TEST(btree_rangefrac_onelevel) {
	TestDest dest;
	TestBtree bt(&dest);

	assertfeq(bt.rangefrac(key(10), key(20)), 0);

	for (int i = 0; i < 100; ++i)
		bt.insert(Vslot(key(i)));
	assert_eq(bt.treelevels, 0);

	assertfeq(bt.rangefrac(key(0), endkey(99)), 1);
	assertfeq(bt.rangefrac(Record(), endkey(99)), 1);
	assertfeq(bt.rangefrac(Record(), endkey(20)), .2);
	assertfeq(bt.rangefrac(key(0), maxkey()), 1);
	assertfeq(bt.rangefrac(Record(), maxkey()), 1);
	assertfeq(bt.rangefrac(key(10), key(20)), .1);
	assertfeq(bt.rangefrac(key(20), endkey(20)), .01);
	assertfeq(bt.rangefrac(Record(), Record()), 0);
	assertfeq(bt.rangefrac(key(999), maxkey()), 0);
}

TEST(btree_rangefrac_multilevel) {
	TestDest dest;
	TestBtree bt(&dest);

	for (int i = 0; i < 100; ++i)
		bt.insert(Vslot(bigkey(i)));
	assert_eq(bt.treelevels, 2);

	assertfeq(bt.rangefrac(key(""), key(0)), 0);
	assertfeq(bt.rangefrac(key(""), key(25)), .25);
	assertfeq(bt.rangefrac(key(""), key(50)), .5);
	assertfeq(bt.rangefrac(key(""), key(75)), .75);
	assertfeq(bt.rangefrac(key(""), key(100)), 1);

	assertfeq(bt.rangefrac(key(0), endkey(99)), 1);
	assertclose(bt.rangefrac(key(10), key(20)), .1);
	assertfeq(bt.rangefrac(key(""), key(20)), .2);
	//		assert_eq(bt.rangefrac(key(20), endkey(20)), .01);
	assertfeq(bt.rangefrac(Record(), Record()), 0);
	assertfeq(bt.rangefrac(key(999), maxkey()), 0);
}
