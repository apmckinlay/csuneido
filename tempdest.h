#pragma once
// Copyright (c) 2002 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <deque>
#include "mmoffset.h"
#include "heap.h"

struct TempNode;

// WARNING: assumes it's safe to convert between pointers and integers
class TempDest {
public:
	TempDest();
	void* allocadr(int n);
	Mmoffset alloc(int n) {
		return off(allocadr(n));
	}
	void free();
	static void* adr(Mmoffset offset) {
		return reinterpret_cast<void*>(offset);
	}
	static Mmoffset off(void* adr) {
		return reinterpret_cast<Mmoffset>(adr);
	}
	void addref(void* p);
	~TempDest();

private:
	Heap heap;
	std::deque<void*> refs;
	int inuse;
};
