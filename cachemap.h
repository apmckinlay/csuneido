#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "std.h"
#include "keyeq.h"

// simple linear search LRU cache, used by rx_compile
// Key and Data must have default constructors
template <int N, class Key, class Data>
class CacheMap {
public:
	CacheMap() : next(0), clock(0) {
	}
	const Data& put(const Key& key, const Data& data);
	Data* get(const Key& key);

private:
	int next;
	uint32_t clock;
	struct Slot {
		Slot() {
		}
		Slot(uint32_t l, Key k, Data d) : lru(l), key(k), data(d) {
		}
		uint32_t lru = 0;
		Key key;
		Data data;
	};
	Slot slots[N];
};

template <int N, class Key, class Data>
const Data& CacheMap<N, Key, Data>::put(const Key& key, const Data& data) {
	if (next < N) {
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
Data* CacheMap<N, Key, Data>::get(const Key& key) {
	for (int i = 0; i < next; ++i)
		if (KeyEq<Key>()(slots[i].key, key)) {
			slots[i].lru = clock++;
			return &slots[i].data;
		}
	return nullptr;
}
