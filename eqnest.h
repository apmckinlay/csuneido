#pragma once
// Copyright (c) 2017 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

class SuValue;

/*
 * Used by SuObject and SuInstance to handle recursive references
 * NOTE: Static so not thread safe. Must not yield within compare.
 */
struct EqNest
	{
	EqNest(const SuValue* x, const SuValue* y)
		{
		if (n >= MAX)
			except("object comparison nesting overflow");
		xs[n] = x;
		ys[n] = y;
		++n;
		}
	static bool has(const SuValue* x, const SuValue* y)
		{
		for (int i = 0; i < n; ++i)
			if ((xs[i] == x && ys[i] == y) || (xs[i] == y && ys[i] == x))
				return true;
		return false;
		}
	~EqNest()
		{
		--n; verify(n >= 0);
		}
	enum { MAX = 20 };
	static int n;
	static const SuValue* xs[MAX];
	static const SuValue* ys[MAX];
	// definitions in suobject.cpp
	// TODO use inline when supported
	};
