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

#include "std.h"

// function object template for hash functions
template <class Key> struct HashFn
	{
	size_t operator()(Key key) const
		{ return key; }
	};

template <> struct HashFn<int64>
	{
	size_t operator()(int64 key) const
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
