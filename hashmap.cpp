// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "hashmap.h"
#include "testing.h"

static int f(int x)
	{ return (x + 7654321) ^ 1234567; }

static int ff(int x)
	{ return (x ^ 1234567) - 7654321; }

TEST(hashmap)
	{
	const int N = 1000;

	typedef HashMap<int,int> htbl;
	htbl tbl;

	srand(123);
	int i;
	for (i = 0; i < N; ++i)
		tbl[f(i)] = rand();
	verify(tbl.size() == N);

	srand(123);
	for (i = 0; i < N; ++i)
		verify(tbl[f(i)] == rand());
	int n = 0;
	for (htbl::iterator p = tbl.begin(); p != tbl.end(); ++p, ++n)
		{
		i = ff(p->key);
		verify(0 <= i && i < N);
		}
	verify(n == N);

	for (i = 0; i < N; ++i)
		verify(tbl.erase(f(i)));
	verify(tbl.empty());
	}
