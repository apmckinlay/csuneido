// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "cachemap.h"

#include "testing.h"

TEST(cachemap)
	{
	CacheMap<4,int,int> cache;
	int i;
	for (i = 0; i < 6; ++i)
		{
		cache.put(i, i * 7);
		int* p = cache.get(i);
		verify(p && *p == i * 7);
		}
	for (i = 0; i < 2; ++i)
		verify(! cache.get(i));
	for (i = 2; i < 6; ++i)
		{
		int* p = cache.get(i);
		verify(p && *p == i * 7);
		}
	verify(! cache.get(6));
	}
