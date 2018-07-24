#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#if _WIN64
typedef unsigned __int64 size_t;
#else
typedef unsigned int size_t;
#endif

extern "C" {
void GC_init();
void* GC_malloc(size_t n);
void* GC_malloc_atomic(size_t n);
void* GC_realloc(void* p, size_t n);
size_t GC_size(void* p);
size_t GC_get_heap_size();
void* GC_base(void* p);
void GC_dump();
void GC_gcollect();

typedef void (*GC_finalization_proc)(void* p, void* data);
void GC_register_finalizer_ignore_self(void* p, GC_finalization_proc fn,
	void* data, GC_finalization_proc* oldfn, void* olddata);
};

inline bool gc_inheap(const void* p) {
	return 0 != GC_base(const_cast<void*>(p));
}

struct NoPtrs {};
extern NoPtrs noptrs;

void* operator new(size_t n, NoPtrs);
void* operator new[](size_t n, NoPtrs);
