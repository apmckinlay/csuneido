#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <utility>
using namespace std::rel_ops;
#include <cstdint>

template <class T1, class T2> inline void construct(T1* p, const T2& value)
	{ (void) new (p) T1(value); }
template <class T> inline void destroy(T* p)
	{ p->~T(); }

template<class T> struct Closer
	{
	Closer(T t) : x(t)
		{ }
	~Closer()
		{ x->close(); }
	T x;
	};

char* dupstr(const char* s);

#if __GNUC__ < 7
#define FALLTHROUGH
#else
#define FALLTHROUGH [[fallthrough]];
#endif
