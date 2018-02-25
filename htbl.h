#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2018 Suneido Software Corp.
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

/*
 * Hmap and Hset are hash based maps and sets
 * implemented with: prime table sizes, open addressing, linear probing,
 * robin hood hashing, and resize on probe limit.
 * Htbl is the common code, it is not intended for external use.
 * To avoid padding it uses an array of blocks each with BN (8) entries.
 * But we still treat it as a single flat array.
 * Within blocks keys, values, and distances are separate arrays to avoid padding.
 * Uses uint16 for size so limited to 64k slots or about 48k elements.
 * Keys and values are assumed to be small and so are passed by value.
 * They are also moved by simple assignment
 * so should not have complex constructors or assignment operators.
 * NOTE: intended for use with garbage collection - does not run destructors.
 * Hashing and key equality functions can be specified with a HfEq type
 * or more "globally" by defining hashfn and keyeq for your type.
 * Allows lookup by a different type from the key,
 * as long as there are compatible hashfn(T) and keyeq(T, Key)
 * See: https://probablydance.com/2017/02/26/i-wrote-the-fastest-hashtable/
 */

#include "except.h"
#include "hashfn.h"
#include "keyeq.h"
#include <utility>
#include <stdint.h>
using std::make_pair;

class Ostream;

const int BN = 8; // number of entries per block, 8 should ensure no padding

static_assert((BN & (BN - 1)) == 0); // should be power of two

// internal base class for Hset and Hmap
template <typename Block, typename Key, typename Val, 
	class Hfn = HashFn<Key>, class Keq = KeyEq<Key>>
class Htbl
	{
	static_assert(sizeof(Block) <= BN * 17); // limit inline size

public:
	Htbl() : blocks(nullptr), isize(0)
		{}
	Htbl(int cap)
		{
		cap = cap + cap / 8; // probelimit will usually hit first
		verify(cap < primes[nprimes - 1]);
		for (isize = 0; primes[isize] < cap; ++isize)
			{ }
		init();
		}
	Htbl(const Htbl& other)
		: blocks(other.blocks), isize(other.isize), siz(other.siz)
		{
		sharewith(other);
		}
	// move constructor
	Htbl(Htbl&& other) : blocks(other.blocks), isize(other.isize),
		readonly(other.readonly), siz(other.siz)
		{
		other.reset();
		}

	Htbl& operator=(const Htbl& other)
		{
		if (&other != this)
			{
			readonly = other.readonly;
			blocks = other.blocks;
			isize = other.isize;
			siz = other.siz;
			sharewith(other);
			}
		return *this;
		}
	// move assignment
	Htbl& operator=(Htbl&& other)
		{
		if (&other != this)
			{
			readonly = other.readonly;
			blocks = other.blocks;
			isize = other.isize;
			siz = other.siz;
			other.reset();
			}
		return *this;
		}

private:
	void sharewith(const Htbl& other) const
		{
		if (!blocks)
			return;
		readonly = other.readonly = true;
		}

public:
	// delete the key, returns true if found, false if not
	bool erase(Key k)
		{
		verify(!readonly);
		if (!siz)
			return false;
		int i = index_for_key(k);
		for (int d = 0; d <= distance(i); ++d, ++i)
			if (Keq()(k, key(i)))
				{
				// shift following elements while it improves placement
				for (; distance(i + 1) > 0; ++i)
					pull(i); // move i+1 to i
				set_distance(i, EMPTY);
				set_key(i, {}); // help garbage collection
				--siz;
				return true;
				}
		return false; // not found
		}
	int size()
		{
		return siz;
		}
	// Make it empty but keep the current capacity
	Htbl& clear()
		{
		verify(!readonly);
		siz = 0;
		memset(blocks, 0, nblocks() * sizeof(Block)); // help garbage collection
		return *this;
		}
	// Reset capacity to zero, release array
	Htbl& reset()
		{
		if (!readonly) // if readonly then could be shared
			memset(blocks, 0, nblocks() * sizeof(Block)); // help garbage collection
		blocks = nullptr;
		isize = siz = 0;
		return *this;
		}
	// returns a move-able copy
	Htbl copy() const
		{
		Htbl tmp;
		tmp.blocks = new Block[nblocks()];
		memcpy(tmp.blocks, blocks, nblocks * sizeof(Block));
		tmp.isize = isize;
		tmp.siz = siz;
		return tmp;
		}
	class iterator
		{
	public:
		iterator(const Htbl* h, int i_, int n_) : htbl(h), i(i_), n(n_)
			{}
		auto operator*()
			{ return htbl->key(i); }
		iterator& operator++()
			{
			for (++i; i < n; ++i)
				if (htbl->distance(i) != EMPTY)
					break;
			return *this;
			}
		bool operator==(const iterator& other) const
			{ return htbl == other.htbl && i == other.i; }
	protected:
		const Htbl* htbl;
		int i;
		int n;
		};
	iterator begin() const
		{
		iterator x(this, -1, nslots());
		return ++x;
		}
	iterator end() const
		{
		return iterator(this, nslots(), nslots());
		}

protected:
	void init()
		{
		verify(isize < nprimes);
		blocks = new Block[nblocks()];
		}
	int nblocks() const
		{
		return ((nslots() - 1) / BN) + 1;
		}
	int nslots() const
		{
		// allocate an extra probelimit slots so we don't need bounds checks
		return primes[isize] + probelimit();
		}
	void insert(Key k, Val v, bool overwrite)
		{
		verify(!readonly);
		if (!blocks)
			init();
		if (siz >= primes[isize] - primes[isize] / 8)
			grow("load");
		for (;;)
			{
			int i = index_for_key(k);
			for (int d = 0; d < probelimit(); ++d, ++i)
				{
				if (distance(i) == EMPTY)
					{
					set_distance(i, d);
					set_key(i, k);
					set_val(i, v);
					++siz;
					return ;
					}
				if (Keq()(k, key(i)))
					{
					if (overwrite)
						set_val(i, v);
					return ; // found existing
					}
				// else collision
				if (distance(i) < d)
					swap(i, d, k, v);
				}
			grow("limit");
			}
		}
	// swap [i] and d,k,v
	void swap(int i, int& d, Key& k, Val& v)
		{
		int dtmp = distance(i);
		set_distance(i, d);
		d = dtmp;

		Key ktmp = key(i);
		set_key(i, k);
		k = ktmp;

		Val vtmp = val(i);
		set_val(i, v);
		v = vtmp;
		}

	// if key is found, returns index, else -1
	template <typename T>
	int find(T k) const
		{
		if (!siz)
			return -1;
		int i = index_for_key(k);
		for (int d = 0; d <= distance(i); ++d, ++i)
			if (Keq()(k, key(i)))
				return i;
		return -1;
		}

	// move [i+1] to [i] - used by delete
	void pull(int i)
		{
		set_distance(i, distance(i + 1) - 1); // distance decreases
		set_key(i, key(i + 1));
		set_val(i, val(i + 1));
		}

	void grow(const char* which)
		{
		// check that we're not growing too much
		verify(siz > primes[isize] / 4);
		Block* oldblocks = blocks;
		int oldnb = nblocks();
		++isize;
		init();
		int oldsiz = siz;
		siz = 0;
		for (int b = 0; b < oldnb; ++b)
			{
			Block& blk = oldblocks[b];
			for (int i = 0; i < BN; ++i)
				if (blk.distance[i] != 0) // NOTE: unadjusted so empty is 0
					{
					fastput(blk.keys[i], blk.val(i));
					}
			}
		verify(siz == oldsiz);
		memset(oldblocks, 0, oldnb * sizeof (Block)); // help garbage collection
		}
	void fastput(Key k, Val v)
		{
		// should be the same as the core of insert
		// but no duplicate checks and no growing
		int i = index_for_key(k);
		for (int d = 0; d < probelimit(); ++d, ++i)
			{
			if (distance(i) == EMPTY)
				{
				set_distance(i, d);
				set_key(i, k);
				set_val(i, v);
				++siz;
				return ;
				}
			if (distance(i) < d)
				swap(i, d, k, v);
			}
		error("grow failed");
		}

	// distance is stored offset by 1 so zeroed data is EMPTY (-1)
	int8_t distance(int i) const
		{ return blocks[i / BN].distance[i % BN] - 1; }
	void set_distance(int i, int8_t d)
		{ blocks[i / BN].distance[i % BN] = d + 1; }

	Key& key(int i) const
		{ return blocks[i / BN].keys[i % BN]; }
	void set_key(int i, Key k)
		{ blocks[i / BN].keys[i % BN] = k; }

	Val val(int i) const
		{ return blocks[i / BN].val(i % BN); }
	void set_val(int i, Val v)
		{ return blocks[i / BN].set_val(i % BN, v); }

	int probelimit() const
		{ return isize <= 1 ? 3 : isize + 4; } // log2(n)

	static constexpr const int primes[] = {
		16 - 3, // 13 + 3 (probelimit) = 16
		32 + 5, // 37 + 3 (probelimit) = 40
		64 - 3,
		128 - 1,
		256 + 1,
		512 - 3,
		1024 - 3,
		2048 + 5,
		4096 - 3,
		8092 - 1,
		16384 - 3,
		32768 + 3,
		65536 - 15
		};
	template <typename T>
	int index_for_key(T k) const
		{
		size_t h = Hfn()(k);
		// return h % primes[isize]; // BUT % is slow, % of constant is faster
		switch (isize)
			{
		case 0: return h % primes[0];
		case 1: return h % primes[1];
		case 2: return h % primes[2];
		case 3: return h % primes[3];
		case 4: return h % primes[4];
		case 5: return h % primes[5];
		case 6: return h % primes[6];
		case 7: return h % primes[7];
		case 8: return h % primes[8];
		case 9: return h % primes[9];
		case 10: return h % primes[10];
		case 11: return h % primes[11];
		case 12: return h % primes[12];
		default: unreachable();
			}
		}

	static const int nprimes = sizeof primes / sizeof(int);
	static const int8_t EMPTY = -1;
	Block* blocks;
	uint8_t isize;
	mutable bool readonly = false;
	uint16_t siz = 0; // current number of items
	};

static_assert(sizeof(Htbl<int, int, int> ) == 8);

template <typename K>
struct HsetBlock
	{
	bool val(int)
		{ return true; }
	void set_val(int, bool)
		{}

	int8_t distance[BN];
	K keys[BN];
	};

// specifies bool for value, but not actually stored, val/set_val are nop
template <typename K, class Hfn = HashFn<K>, class Keq = KeyEq<K>>
class Hset : public Htbl<HsetBlock<K>, K, bool, Hfn, Keq>
	{
public:
	using Tbl = Htbl<HsetBlock<K>, K, bool, Hfn, Keq>;

	using Tbl::Tbl;

	template <typename T>
	K* find(T k)
		{
		int i = Tbl::find(k);
		return i < 0 ? nullptr : &Tbl::key(i);
		}
	void insert(K k)
		{ Tbl::insert(k, true, false); }
	};

template <typename K>
Ostream& operator<<(Ostream& os, const Hset<K>& hset)
	{
	os << "{";
	auto sep = "";
	for (auto k : hset)
		{
		os << sep << k;
		sep = ", ";
		}
	os << "}";
	return os;
	}

template <typename K, typename V>
struct HmapBlock
	{
	// ReSharper disable once Cpp
	V val(int i)
		{ return vals[i]; }
	void set_val(int i, V v)
		{ vals[i] = v; }

	int8_t distance[BN];
	K keys[BN];
	V vals[BN];
	};

template <typename K, typename V, class Hfn = HashFn<K>, class Keq = KeyEq<K>>
class Hmap : public Htbl<HmapBlock<K, V>, K, V, Hfn, Keq>
	{
public:
	using Tbl = Htbl<HmapBlock<K, V>, K, V, Hfn, Keq>;
	using Iter = typename Tbl::iterator;

	using Tbl::Tbl;

	template <typename T>
	V* find(T k)
		{
		int i = Tbl::find(k);
		return i < 0 ? nullptr : &valref(i);
		}
	V& operator[](K k)
		{
		this->insert(k, {}, false);
		return *find(k);
		}
	void put(K k, V v)
		{
		this->insert(k, v, true);
		}
	class iterator : public Iter
		{
	public:
		iterator(Iter it) : Iter(it)
			{}
		auto operator*()
			{ return make_pair(Iter::operator*(), ((Hmap*) this->htbl)->val(this->i)); }
		};
	iterator begin() const
		{ return iterator(Tbl::begin()); }
	iterator end() const
		{ return iterator(Tbl::end()); }
private:
	V& valref(int i)
		{ return this->blocks[i / BN].vals[i % BN]; }
	};

template <typename K, typename V>
Ostream& operator<<(Ostream& os, Hmap<K,V>& hmap)
	{
	os << "{";
	auto sep = "";
	for (auto [k, v] : hmap)
		{
		os << sep << k << ": " << v;
		sep = ", ";
		}
	os << "}";
	return os;
	}
