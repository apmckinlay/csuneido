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

// tests ------------------------------------------------------------

#pragma warning (default : 4018 4267 4310)

#include "htbl.h"

#include <tuple>
#include "testing.h"
#include "ostreamstr.h"
#include "gcstring.h"
#include "hashfn.h"

// for TEST(1, hset1)
struct Foo
	{
	const char* s;
	int n;
	};
template<> struct HashFn<Foo>
	{
	size_t operator()(const Foo& k) const
		{ return hashfn(k.s); }
	size_t operator()(const char* s) const
		{ return hashfn(s); }
	};
template<> struct KeyEq<Foo>
	{
	bool operator()(const char* x, const Foo& y) const
		{ return 0 == strcmp(x, y.s); }
	bool operator()(const Foo& x, const Foo& y) const
		{ return 0 == strcmp(x.s, y.s); 	}
	};

// for TEST(2, hset2)
struct Bar
	{
	const char* s;
	int n;
	};
struct BarHash
	{
	size_t operator()(const char* k)
		{ return ::hashfn(k); }
	size_t operator()(const Bar& k)
		{ return ::hashfn(k.s); 	}
	};
struct BarEq
	{
	bool operator()(const char* x, const Bar& y)
		{ return 0 == strcmp(x, y.s); }
	bool operator()(const Bar& x, const Bar& y)
		{ return 0 == strcmp(x.s, y.s); }
	};

class test_htbl : public Tests
	{
	TEST(0, hset)
		{
		// NOTE: table size of 61 so probe limit doesn't cause resize
		// assumes that int hash is identity, not mixed
		Hset<int> hset(40);
		const int P = 61;
		asserteq(0, hset.size());
		std::tuple<int, const char*> add[] = {
			{2,				"{2}"},
			{P + 2,			"{2, 63}"}, // collision
			{P + P + 2,		"{2, 63, 124}"}, // collision
			{3,				"{2, 63, 124, 3}"}, // after collisions
			{1,				"{1, 2, 63, 124, 3}"}, // before
			{P + 1,			"{1, 62, 63, 124, 2, 3}"}, // bump past group
			};
		for (auto [n,s] : add)
			{
			hset.insert(n);
			asserteq(s, str(hset));
			}
		asserteq(6, hset.size());
		for (auto [n,_] : add)
			{
			auto p = hset.find(n);
			verify(p);
			asserteq(n, *p);
			(void)_;
			}
		std::tuple<int, const char*> del[] = {
			{ P + 1,			"{1, 63, 124, 2, 3}" },
			{ 1,				"{63, 124, 2, 3}" },
			{ 3,				"{63, 124, 2}" },
			{ P + 2,			"{124, 2}" },
			{ 2,				"{124}" },
			{ P + P + 2,		"{}" }, 
			};
		for (auto[n, s] : del)
			{
			verify(hset.erase(n));
			asserteq(s, str(hset));
			}
		asserteq(0, hset.size());
		}
	static gcstring str(const Hset<int>& hset)
		{
		OstreamStr os;
		os << hset;
		return os.gcstr();
		}
	TEST(1, hset1)
		{
		// test get by different type (char*) than key (Foo)
		// using standard hashfn and operator==
		Hset<Foo> hset;
		hset.insert(Foo{"one",123});
		auto p = hset.find("one");
		verify(p);
		asserteq(123, p->n);
		}
	TEST(2, hset2)
		{
		// test get by different type (char*) than key (Bar)
		// using explicit custom HfEq 
		Hset<Bar, BarHash, BarEq> hset;
		hset.insert(Bar{"one",123});
		auto p = hset.find("one");
		verify(p);
		asserteq(123, p->n);
		}
	TEST(3, hmap)
		{
		Hmap<int, int> hmap;
		verify(! hmap.find(0));
		for (int i = 0; i < 10; ++i)
			hmap.put(i, 100 + i);
		for (int i = 0; i < 10; ++i)
			{
			int* pi = hmap.find(i);
			verify(pi);
			asserteq(100 + i, *pi);
			asserteq(100 + i, hmap[i]);
			}
		asserteq(10, hmap.size());
		OstreamStr os;
		os << hmap;
		asserteq("{0: 100, 1: 101, 2: 102, 3: 103, 4: 104, 5: 105, 6: 106, "
			"7: 107, 8: 108, 9: 109}", os.gcstr());
		// note: order depends on hashing
		}
	TEST(4, randmap)
		{
		const int N = 40000;

		Hmap<int,int> tbl;

		for (int i = 0; i < N; ++i)
			{
			//tbl.put(mix(i), i);
			tbl[mix(i)] = i;
			}
		asserteq(N, tbl.size());

		for (int i = 0; i < N; ++i)
			{
			auto p = tbl.find(mix(i));
			verify(p);
			asserteq(i, *p);
			asserteq(i, tbl[mix(i)]);
			}

		int n = 0;
		for (auto [k,v] : tbl)
			{
			asserteq(k, mix(v));
			++n;
			}
		verify(n == N);

		for (int i = 0; i < N; i += 2)
			verify(tbl.erase(mix(i)));
		asserteq(N/2, tbl.size());
		for (int i = 0; i < N; ++i)
			if (i & 1)
				asserteq(i, tbl[mix(i)]);
			else
				verify(!tbl.find(mix(i)));

		n = 0;
		for (auto[k, v] : tbl)
			{
			asserteq(k, mix(v));
			++n;
			}
		verify(n == N/2);
		}
	static int mix(uint32_t n)
		{
		n = ~n + (n << 15);
		n = n ^ (n >> 12);
		n = n + (n << 2);
		n = n ^ (n >> 4);
		n = n * 2057;
		n = n ^ (n >> 16);
		return n;
		}
	TEST(5, set_move)
		{
		Hset<int> t;
		t.insert(123);
		auto t2 = t; // sets both to readonly since shared data
		xassert(t.insert(789));
		xassert(t2.insert(789));

		auto t3{ f() }; // moved so not readonly
		t3.insert(56);

		Hset<int> t4;
		t4 = f(); // moved so not readonly
		t4.insert(123);
		}
	static Hset<int> f()
		{
		Hset<int> tmp;
		tmp.insert(12);
		return tmp;
		}
	TEST(6, map_move)
		{
		Hmap<int,int> t;
		t.put(1, 111);
		auto t2 = t; // sets both to readonly since shared data
		xassert(t.put(2, 222));
		xassert(t2.put(3, 333));

		auto t3{ m() }; // moved so not readonly
		t3.put(4, 444);

		Hmap<int,int> t4;
		t4 = m(); // moved so not readonly
		t4.put(5, 555);
		}
	static Hmap<int,int> m()
		{
		Hmap<int,int> tmp;
		tmp.put(9, 999);
		return tmp;
		}
	};
REGISTER(test_htbl);
