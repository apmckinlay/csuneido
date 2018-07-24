// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "permheap.h"
#include "except.h"
#include "port.h"

PermanentHeap::PermanentHeap(const char* n, int size) : name(n) {
	start = mem_uncommitted(size);
	next = (char*) start;
	commit_end = next;
	limit = next + size;
}

// NOTE: PAGESIZE must be a power of 2
const int PAGESIZE = 16 * 1024;
inline int roundup(int n) {
	return ((n - 1) | (PAGESIZE - 1)) + 1;
}
inline void* roundup(void* n) {
	return (void*) ((((int) n - 1) | (PAGESIZE - 1)) + 1);
}

void* PermanentHeap::alloc(int size) {
	void* p = next;
	next += size;
	if (next > commit_end) {
		if (next > limit)
			except("out of space for " << name);
		// commit some more
		int n = (char*) roundup(next) - commit_end;
		mem_commit(commit_end, n);
		commit_end += n;
	}
	return p;
}

const int DECOMMIT_THRESHOLD = 1024 * 1024; // 1mb

void PermanentHeap::free(void* p) {
	verify(start <= p && p <= next);
	next = (char*) p;
	p = roundup(p);
	int n = commit_end - (char*) p;
	if (n > DECOMMIT_THRESHOLD) {
		mem_decommit(p, n);
		commit_end -= n;
	}
}

// test =============================================================

#include "testing.h"
#include "random.h"

TEST(permheap) {
	PermanentHeap ph("test", 1024 * 1024);
	for (int i = 0; i < 100; ++i) {
		int n = random(10000);
		void* p = ph.alloc(n);
		memset(p, 0, n);
	}
}
TEST(permheap_free) {
	PermanentHeap ph("test", 8 * 1024 * 1024);
	void* p = ph.alloc(8);
	void* q = ph.alloc(2 * 1024 * 1024);
	ph.free(q);
	memset(p, 0, 1);
}
