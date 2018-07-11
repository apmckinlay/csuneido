// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "array.h"
#include "testing.h"

TEST(array)
	{
	Array<int, 11> a;

	for (int i = 1; i < 10; i += 2)
		a.append(i);

	verify(a.size() == 5);

	a.insert(6);
	a.insert(10);
	a.insert(2);
	a.insert(4);
	a.insert(0);
	a.insert(8);

	verify(a.size() == 11);
	verify(a.capacity() == 11);
	a.erase(4);
	a.erase(5);
	verify(a.size() == 9);
	}
