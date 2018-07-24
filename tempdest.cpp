// Copyright (c) 2002 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "tempdest.h"
#include "except.h"
#include <algorithm>
#include "gc.h"

int tempdest_inuse = 0;

TempDest::TempDest() : inuse(0) {
}

void TempDest::addref(void* p) {
	if (gc_inheap(p))
		refs.push_back(p);
}

void* TempDest::allocadr(int n) {
	++tempdest_inuse;
	++inuse;
	return static_cast<TempNode*>(heap.alloc(n));
}

void TempDest::free() {
	heap.destroy();
	tempdest_inuse -= inuse;
	inuse = 0;
	std::fill(refs.begin(), refs.end(), (void*) 0); // to help gc
	refs.clear();
}

TempDest::~TempDest() {
	free();
}

//-------------------------------------------------------------------

#include "testing.h"

TEST(tempdest) {
	int orig_alloced = tempdest_inuse;
	TempDest td;
	td.allocadr(4000);
	td.allocadr(4000);
	td.free();
	verify(tempdest_inuse == orig_alloced);
}
TEST(tempdest_capacity) {
	TempDest td;
	for (int i = 0; i < 100 * 1024; ++i)
		td.alloc(4096);
}
