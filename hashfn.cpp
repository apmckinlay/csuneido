// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "hashfn.h"

// FNV1a see: 
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function

size_t hashfn(const char* s)
	{
	size_t hash = 2166136261;
	for (; *s; ++s)
		{
		hash = hash ^ *s;
		hash = hash * 16777619;
		}
	return hash;
	}

size_t hashfn(const char* s, int n)
	{
	size_t hash = 2166136261;
	for (auto end = s + n; s < end; ++s)
		{
		hash = hash ^ *s;
		hash = hash * 16777619;
		}
	return hash;
	}

#include "testing.h"

TEST(hashfn_string)
	{
	auto s = "one";
	auto h = hashfn(s);
	// both are equivalent
	assert_eq(h, hashfn(s, strlen(s)));
	// order dependent
	assert_neq(h, hashfn("eno"));
	}
