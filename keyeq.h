#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <string.h>

// default key comparison functions

template <class Key> struct KeyEq
	{
	bool operator()(const Key& x, const Key& y) const
		{ return x == y; }
	};

template <> struct KeyEq<char*>
	{
	bool operator()(const char* x, const char* y) const
		{ return 0 == strcmp(x, y); }
	};

template <> struct KeyEq<const char*>
	{
	bool operator()(const char* x, const char* y) const
		{ return 0 == strcmp(x, y); }
	};
