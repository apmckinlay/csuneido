#pragma once
// Copyright (c) 2014 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "gc.h"

#define ALLOCA_LIMIT (64 * 1024) // 64 kb

// WARNING: n is evaluated multiple times
// WARNING: result is assumed to not contain pointers (for GC)
#define tmpalloc(n) ((n < ALLOCA_LIMIT) ? (char*) _alloca(n) : new(noptrs) char[n])
