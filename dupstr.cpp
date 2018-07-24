// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "std.h"
#include <cstring>
#include "gc.h"

// a version of dupstr that uses operator new instead of malloc

char* dupstr(const char* s) {
	if (s == nullptr)
		return nullptr;
	size_t n = strlen(s) + 1;
	char* t = new (noptrs) char[n];
	return (char*) memcpy(t, s, n);
}
