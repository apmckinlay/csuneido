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
#include "keyeq.h"

// simple linear search LRU cache, used by rx_compile
// Key and Data must have default constructors
template <int N, class Key, class Data> class CacheMap
	{
public:
	CacheMap() : next(0), clock(0)
		{ }
	const Data& put(const Key& key, const Data& data);
	Data* get(const Key& key);
private:
	int next;
	ulong clock;
	struct Slot
		{
		Slot()
			{ }
		Slot(ulong l, Key k, Data d) : lru(l), key(k), data(d)
			{ }
		ulong lru = 0;
		Key key;
		Data data;
		};
	Slot slots[N];
	};

template <int N, class Key, class Data>
const Data& CacheMap<N,Key,Data>::put(const Key& key, const Data& data)
	{
	if (next < N)
		{
		slots[next++] = Slot(clock++, key, data);
		return data;
		}
	int lru = 0;
	for (int i = 0; i < next; ++i)
		if (slots[i].lru < slots[lru].lru)
			lru = i;
	slots[lru] = Slot(clock++, key, data);
	return data;
	}

template <int N, class Key, class Data>
Data* CacheMap<N,Key,Data>::get(const Key& key)
	{
	for (int i = 0; i < next; ++i)
		if (KeyEq<Key>()(slots[i].key, key))
			{
			slots[i].lru = clock++;
			return &slots[i].data;
			}
	return nullptr;
	}
