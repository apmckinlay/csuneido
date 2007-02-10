#ifndef STD_H
#define STD_H

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
#if __GNUC__ != 2
using namespace std::rel_ops;
#endif

#ifdef _MSC_VER
#pragma warning(4 : 4786)
#endif

#if __GNUC__ != 2
template <class T1, class T2> inline void construct(T1* p, const T2& value)
	{ (void) new (p) T1(value); }
template <class T> inline void destroy(T* p)
	{ p->~T(); }
#endif

typedef signed char schar;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;

#ifdef _MSC_VER
typedef _int64 int64;
typedef unsigned _int64 uint64;
#else
typedef long long int64;
typedef unsigned long long uint64;
#endif

#if defined(_MSC_VER)
#define NORETURN(fn) void _declspec(noreturn) fn
#elif defined(__GNUC__)
#define NORETURN(fn) void fn __attribute__ ((noreturn))
#else
#define NORETURN(fn) fn
#endif

#endif