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

TEST(slots_vslot)
	{
	Mmoffset n = (int64) 3 * 1024 * 1024 * 1024;
	Record r;
	r.addmmoffset(n);
	assert_eq(r.getmmoffset(0), n);
	Vslot vs(r);
	assert_eq(vs.adr(), n);
	}
TEST(slots_vfslot)
	{
	Record r;
	r.addval("hello");
	Mmoffset n = (int64) 3 * 1024 * 1024 * 1024;
	VFslot vfs(r, n);
	assert_eq(vfs.key, r);
	assert_eq(vfs.adr, n);

	VFslots slots;
	slots.push_back(vfs);
	VFslot vfs2 = slots.back();
	assert_eq(vfs2.key, r);
	assert_eq(vfs2.adr, n);
	}
