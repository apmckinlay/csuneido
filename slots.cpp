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

#include "slots.h"
#include "std.h"
#include "testing.h"

class test_slots : public Tests
	{
	TEST(0, "Vslot")
		{
		Mmoffset n = (int64) 3 * 1024 * 1024 * 1024;
		Record r;
		r.addmmoffset(n);
		asserteq(r.getmmoffset(0), n);
		Vslot vs(r);
		asserteq(vs.adr(), n);
		}
	TEST(1, "VFslot")
		{
		Record r;
		r.addval("hello");
		Mmoffset n = (int64) 3 * 1024 * 1024 * 1024;
		VFslot vfs(r, n);
		asserteq(vfs.key, r);
		asserteq(vfs.adr, n);

		VFslots slots;
		slots.push_back(vfs);
		VFslot vfs2 = slots.back();
		asserteq(vfs2.key, r);
		asserteq(vfs2.adr, n);
		}
	};
REGISTER(test_slots);
