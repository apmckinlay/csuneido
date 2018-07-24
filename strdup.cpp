// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <string.h>
#include "gc.h"

// a version of strdup that uses operator new instead of malloc

char* strdup(const char* s) {
	size_t n = strlen(s) + 1;
	char* t = new (noptrs) char[n];
	return (char*) memcpy(t, s, n);
}
