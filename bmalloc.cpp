// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "gc.h"
#include "fatal.h"

NoPtrs noptrs;

inline void* ck(void* p) {
	if (p == nullptr)
		fatal("out of memory");
	return p;
}

void* operator new(size_t n) {
	return ck(GC_malloc(n));
}

void* operator new(size_t n, NoPtrs) {
	return ck(GC_malloc_atomic(n));
}

void* operator new[](size_t n) {
	return ck(GC_malloc(n));
}

void* operator new[](size_t n, NoPtrs) {
	return ck(GC_malloc_atomic(n));
}

void operator delete(void*) noexcept {
}

void operator delete(void*, unsigned int) noexcept {
}

void operator delete[](void*) noexcept {
}

void operator delete[](void*, unsigned int) noexcept {
}
