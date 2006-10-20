/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2003 Suneido Software Corp. 
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

#include "bitmap.h"
#include "except.h"

static inline void wordset(word* p, int x, int n)
	{
	word* limit = p + n;
	while (p < limit)
		*p++ = x;
	}

// + 1 for sentinel
Bitmap::Bitmap(int n, void* p)
	: data(p ? (word*) p : new word[n / NBITS + 1]), size(n), limit(n)
	{
	verify((n & MASK) == 0);
	wordset(data, 0, n / NBITS + 1);
	}

void Bitmap::clear(int x)
	{ 
	wordset(data, x ? ~0 : 0, limit / NBITS);
	}

void Bitmap::slowset1(int from, int to)
	{
	for (int i = from; i < to; ++i)
		set1(i);
	}

void Bitmap::slowset0(int from, int to)
	{
	for (int i = from; i < to; ++i)
		set0(i);
	}

int Bitmap::slownext1(int i)
	{
	// BUG: if i is limit - 1 then accesses get(limit) which is invalid
	while (i < limit && get(++i) == 0)
		;
	return i;
	}

int Bitmap::slownext0(int i)
	{
	// BUG: if i is limit - 1 then accesses get(limit) which is invalid
	while (i < limit && get(++i) == 1)
		;
	return i;
	}

void Bitmap::set_limit(int n)
	{
	n = ((n - 1) | MASK) + 1;
	verify(n <= size);
	limit = n;
	}
	
#ifndef __GNUC__
#define __attribute__(stuff)
#endif

static inline word set1bits(word x, int from, int to) __attribute__((always_inline));
static inline word set1bits(word x, int from, int to)
	{
	if (to - from == 31)
		return ~0;
	return x | (~((word) ~0 << (to - from + 1)) << from);
	}

void Bitmap::set1(int from, int to)
	{
	if (from == to--)
		return ;
	int first = from >> SHIFT;
	int last = to >> SHIFT;
	if (first == last)
		{
		data[first] = set1bits(data[first], from & MASK, to & MASK);
		}
	else
		{
		data[first] = set1bits(data[first], from & MASK, NBITS - 1);
		wordset(data + first + 1, ~0, last - first - 1);
		data[last] = set1bits(data[last], 0, to & MASK);
		}
	}

static inline int set0bits(word x, int from, int to) __attribute__((always_inline));
static inline int set0bits(word x, int from, int to)
	{
	if (to - from == 31)
		return 0;
	return x &= ~(~((word) ~0 << (to - from + 1)) << from);
	}

void Bitmap::set0(int from, int to)
	{
	if (from == to--)
		return ;
	int first = from >> SHIFT;
	int last = to >> SHIFT;
	if (first == last)
		{
		data[first] = set0bits(data[first], from & MASK, to & MASK);
		}
	else
		{
		data[first] = set0bits(data[first], from & MASK, NBITS - 1);
		wordset(data + first + 1, 0, last - first - 1);
		data[last] = set0bits(data[last], 0, to & MASK);
		}
	}
	
static inline word* find_one_bit(word* p, int n) __attribute__((always_inline));
static inline word* find_one_bit(word* p, int n)
	{
	// WARNING: p[n] must be valid
	p[n] = 1; // sentinel
	while (*p++ == 0)
		{}
	return p - 1;
	}

// Number of Least Significant zeros
static inline int nls0(word x) __attribute__((always_inline));
static inline int nls0(word x)
	{
	if (x == 0)
		return 32;
	int n = 1;
	if ((x & 0xffff) == 0) { n += 16; x >>= 16; }
	if ((x & 0xff) == 0) { n += 8; x >>= 8; }
	if ((x & 0xf) == 0) { n += 4; x >>= 4; }
	if ((x & 0x3) == 0) { n += 2; x >>= 2; }
	return n - (x & 1);
	}
	
int Bitmap::next1(int from)
	{
	if (++from >= limit)
		return limit;
	word* p = data + (from >> SHIFT);
	int j = nls0(*p >> (from & MASK));
	if (j < NBITS)
		return from + j;
	p = find_one_bit(p + 1, ((limit >> SHIFT) - (from >> SHIFT)) - 1);
	return ((p - data) << SHIFT) + nls0(*p);
	}

static inline word* find_zerobit(word* p, int n) __attribute__((always_inline));
static inline word* find_zerobit(word* p, int n)
	{
	// WARNING: p[n] must be valid
	p[n] = 0; // sentinel
	while (~*p++ == 0)
		{}
	return p - 1;
	}

int Bitmap::next0(int from)
	{
	if (++from >= limit)
		return limit;
	word* p = data + (from >> SHIFT);
	int j = nls0(~*p >> (from & MASK));
	if (j < NBITS)
		return from + j;
	p = find_zerobit(p + 1, ((limit >> SHIFT) - (from >> SHIFT)) - 1);
	return ((p - data) << SHIFT) + nls0(~*p);
	}

int Bitmap::prev1(int i)
	{
	while (i > 0 && get(i) == 0)
		--i;
	return i;
	}
	
#include "testing.h"
#include "random.h"

#include <algorithm>
	
class test_bitmap : public Tests
	{
	TEST(0, main)
		{
		const int N = 1024;
		Bitmap bm(N);
		int i;
		for (i = 0; i < N; ++i)
			asserteq(bm.get(i), 0);
		srand(1);
		for (i = 0; i < N / 2; ++i)
			bm.set1(random(N));
		srand(1);
		for (i = 0; i < N / 2; ++i)
			asserteq(bm.get(random(N)), 1);
		}
	TEST(1, next)
		{
		const int N = 320;
		Bitmap bm(N);
		bm.clear();
		bm.set1(30);
		bm.set1(90);
		asserteq(bm.next1(27), 30); // same byte
		asserteq(bm.next1(5), 30);
		asserteq(bm.next1(30), 90);
		asserteq(bm.next1(100), N);
			
		bm.clear(1);
		bm.set0(30);
		bm.set0(90);
		asserteq(bm.next0(27), 30); // same byte
		asserteq(bm.next0(5), 30);
		asserteq(bm.next0(30), 90);
		asserteq(bm.next0(100), N);

		for (int j = 0; j < 100; ++j)
			{
			int i;
			for (i = 0; i < 4; ++i)
				{
				bm.data[random(N / bm.NBITS)] = 0;
				bm.data[random(N / bm.NBITS)] = rand();
				bm.data[random(N / bm.NBITS)] = ~0;
				}
			for (i = 0; i <= N; ++i)
				{
				asserteq(bm.next0(i), bm.slownext0(i));
				asserteq(bm.next1(i), bm.slownext1(i));
				}
			}
		}
	TEST(2, set)
		{
		const int N = 4 * 32;
		Bitmap bm(N);
		Bitmap b2(N);
		int i;

		bm.clear();
		bm.set1(4, 68);
		asserteq(bm.data[0], 0xfffffff0);
		asserteq(bm.data[1], 0xffffffff);
		asserteq(bm.data[2], 0x0f);

		bm.clear();
		bm.set1(0, 1);
		bm.set1(34, 35);
		bm.set1(69, 70);
		bm.set1(127, 128);
		asserteq(bm.data[0], 1);
		asserteq(bm.data[1], 4);
		asserteq(bm.data[2], 0x20);
		asserteq(bm.data[3], 0x80000000);

		for (i = 0; i < 100; ++i)
			{
			int from = random(N);
			int to = random(N);
			if (to < from)
				std::swap(to, from);
			bm.clear();
			bm.set1(from, to);
			b2.clear();
			b2.slowset1(from, to);
			for (int j = 0; j < N / bm.NBITS; ++j)
				verify(bm.data[j] == b2.data[j]);
			}

		bm.clear(1);
		bm.set0(4, 68);
		verify(bm.data[0] == 0xf);
		verify(bm.data[1] == 0);
		verify(bm.data[2] == 0xfffffff0);

		bm.clear(1);
		bm.set0(0, 1);
		bm.set0(34, 35);
		bm.set0(69, 70);
		bm.set0(127, 128);
		asserteq(bm.data[0], ~1U);
		asserteq(bm.data[1], ~4U);
		asserteq(bm.data[2], ~0x20U);
		asserteq(bm.data[3], ~0x80000000);

		for (i = 0; i < 100; ++i)
			{
			int from = random(N);
			int to = random(N);
			if (to < from)
				std::swap(to, from);
			bm.clear(1);
			bm.set0(from, to);
			b2.clear(1);
			b2.slowset0(from, to);
			for (int j = 0; j < N / bm.NBITS; ++j)
				verify(bm.data[j] == b2.data[j]);
			}
		}
	TEST(3, find)
		{
		const int N = 10;
		word data[N + 1]; // allow for sentinel
		wordset(data, 0, N);
		verify(find_one_bit(data, N) == data + N);
		data[5] = 1;
		verify(find_one_bit(data, N) == data + 5);
		data[0] = 1;
		verify(find_one_bit(data, N) == data);

		wordset(data, 0xffffffff, N);
		verify(find_zerobit(data, N) == data + N);
		data[5] = 0;
		verify(find_zerobit(data, N) == data + 5);
		data[0] = 0;
		verify(find_zerobit(data, N) == data);
		}
	TEST(4, nls0)
		{
		asserteq(nls0(0), 32);
		for (int i = 0; i < 32; ++i)
			asserteq(nls0(1 << i), i);
		}
	TEST(5, setbits)
		{
		asserteq(set1bits(0, 0, 0), 1);
		asserteq(set1bits(0, 0, 1), 3);
		asserteq(set1bits(0, 31, 31), 0x80000000);
		asserteq(set1bits(0, 8, 23), 0x00ffff00);
		asserteq(set1bits(0x80000001, 8, 23), 0x80ffff01);
		asserteq(set1bits(0, 0, 31), 0xffffffff);
			
		asserteq(set0bits(~0, 0, 0), ~1);
		asserteq(set0bits(~0, 0, 1), ~3);
		asserteq(set0bits(~0, 31, 31), ~0x80000000);
		asserteq(set0bits(~0, 8, 23), ~0x00ffff00);
		asserteq(set0bits(~0x80000001, 8, 23), ~0x80ffff01);
		asserteq(set0bits(~0, 0, 31), 0);
		}
	};
REGISTER(test_bitmap);
