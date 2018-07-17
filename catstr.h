#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// ReSharper disable once CppUnusedIncludeDirective
#include <malloc.h> // for _alloca
#include <cstring>

char* catstr(char *dst, ...);

#define CATSTRA(s, t) \
	catstr((char*) _alloca(strlen(s) + strlen(t) + 1), s, t, 0)
#define CATSTR3(s1, s2, s3) \
	catstr((char*) _alloca(strlen(s1) + strlen(s2) + strlen(s3) + 1), s1, s2, s3, 0)
#define STRDUPA(s) \
	strcpy((char*) _alloca(strlen(s) + 1), s)
#define PREFIXA(s, n) \
	strcpyn((char*) _alloca((n) + 1), s, n)

inline char* strcpyn(char* dst, const char* src, int n)
	{
	memcpy(dst, src, n);
	dst[n] = 0;
	return dst;
	}
