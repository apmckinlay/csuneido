/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2005 Suneido Software Corp.
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

#include "btree.h"
#include "slots.h"
#include "testing.h"
#include <vector>

class TestDest
	{
public:
	Mmoffset alloc(int n)
		{
		void* p = new char[n];
		blocks.push_back(p); // protect from garbage collect
		return reinterpret_cast<Mmoffset>(p);
		}
	void free()
		{ }
	static void* adr(Mmoffset offset)
		{ return reinterpret_cast<void*>(offset); }
	static Mmoffset off(void* adr)
		{ return reinterpret_cast<Mmoffset>(adr); }
	void addref(void* p)
		{ }
	std::vector<void*> blocks;
	};

typedef Btree<Vslot,VFslot,Vslots,VFslots,TestDest> TestBtree;

#include "value.h"
#include <math.h>

#define assertfeq(x, y) \
	do { float x_ = x; float y_ = (float) y; \
	except_if(fabs(x_ - y_) > .05f, \
	__FILE__ << ':' << __LINE__ << ": " << "error: " << #x << " (" << x_ << \
	") not close to " << #y << " (" << y_ << ")"); } while (false)
#define assertclose(x, y)	verify(fabs((x) - (y)) < .35)

class test_btree : public Tests
	{
	TEST(0, rangefrac_onelevel)
		{
		TestDest dest;
		TestBtree bt(&dest);

		assertfeq(bt.rangefrac(key(10), key(20)), 0);

		for (int i = 0; i < 100; ++i)
			bt.insert(Vslot(key(i)));
		asserteq(bt.treelevels, 0);

		assertfeq(bt.rangefrac(key(0), endkey(99)), 1);
		assertfeq(bt.rangefrac(Record(), endkey(99)), 1);
		assertfeq(bt.rangefrac(Record(), endkey(20)), .2);
		assertfeq(bt.rangefrac(key(0), maxkey()), 1);
		assertfeq(bt.rangefrac(Record(), maxkey()), 1);
		assertfeq(bt.rangefrac(key(10), key(20)), .1);
		assertfeq(bt.rangefrac(key(20), endkey(20)), .01);
		assertfeq(bt.rangefrac(Record(), Record()), 0);
		assertfeq(bt.rangefrac(key(999), maxkey()), 0);
		}
	TEST(1, rangefrac_multilevel)
		{
		TestDest dest;
		TestBtree bt(&dest);

		for (int i = 0; i < 100; ++i)
			bt.insert(Vslot(bigkey(i)));
		asserteq(bt.treelevels, 2);

		assertfeq(bt.rangefrac(key(""), key(0)), 0);
		assertfeq(bt.rangefrac(key(""), key(25)), .25);
		assertfeq(bt.rangefrac(key(""), key(50)), .5);
		assertfeq(bt.rangefrac(key(""), key(75)), .75);
		assertfeq(bt.rangefrac(key(""), key(100)), 1);

		assertfeq(bt.rangefrac(key(0), endkey(99)), 1);
		assertclose(bt.rangefrac(key(10), key(20)), .1);
		assertfeq(bt.rangefrac(key(""), key(20)), .2);
//		asserteq(bt.rangefrac(key(20), endkey(20)), .01);
		assertfeq(bt.rangefrac(Record(), Record()), 0);
		assertfeq(bt.rangefrac(key(999), maxkey()), 0);
		}
	static Record key(int i)
		{
		Record r;
		r.addval(Value(i));
		return r;
		}
	static Record key(const char* s)
		{
		Record r;
		r.addval(s);
		return r;
		}
	Record endkey(int i) const
		{
		Record r = key(i);
		r.addmax();
		return r;
		}
	static Record maxkey()
		{
		Record r;
		r.addmax();
		return r;
		}
	static Record bigkey(int i)
		{
		static gcstring filler = make_filler();
		Record r;
		r.addval(Value(i));
		r.addval(filler);
		return r;
		}
	static gcstring make_filler()
		{
		const int N = 500;
		char* buf = salloc(N);
		memset(buf, ' ', N);
		return gcstring::noalloc(buf, N);
		}
	};
REGISTER(test_btree);
