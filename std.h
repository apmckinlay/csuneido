#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2000 Suneido Software Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation - version 2.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License in the file COPYING
 * for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <utility>
using namespace std::rel_ops;

template <class T1, class T2> inline void construct(T1* p, const T2& value)
	{ (void) new (p) T1(value); }
template <class T> inline void destroy(T* p)
	{ p->~T(); }

//TODO remove these and use (u)int_(8|16|32|64)_t
typedef signed char schar;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef long long int64;
typedef unsigned long long uint64;

template<class T> struct Closer
	{
	Closer(T t) : x(t)
		{ }
	~Closer()
		{ x->close(); }
	T x;
	};

char* dupstr(const char* s);

#ifdef __GNUC__
#define FALLTHROUGH
#else
#define FALLTHROUGH [[fallthrough]];
#endif
