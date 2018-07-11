#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "std.h"

// function object template for hash functions
template <class Key> struct HashFn
	{
	size_t operator()(Key key) const
		{ return key; }
	};

template <> struct HashFn<int64_t>
	{
	size_t operator()(int64_t key) const
		{ return static_cast<size_t>(key ^ (key >> 32)); }
	};

// hash char* that's nul terminated
size_t hashfn(const char* s);

template <> struct HashFn<char*>
	{
	size_t operator()(char* key) const
		{ return hashfn(key); }
	};

template <> struct HashFn<const char*>
	{
	size_t operator()(const char* key) const
		{ return hashfn(key); }
	};

// hash char* given length
size_t hashfn(const char* s, int n);
