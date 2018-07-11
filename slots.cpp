// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "slots.h"
#include "std.h"
#include "testing.h"

TEST(slots_vslot)
	{
	Mmoffset n = (int64_t) 3 * 1024 * 1024 * 1024;
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
	Mmoffset n = (int64_t) 3 * 1024 * 1024 * 1024;
	VFslot vfs(r, n);
	assert_eq(vfs.key, r);
	assert_eq(vfs.adr, n);

	VFslots slots;
	slots.push_back(vfs);
	VFslot vfs2 = slots.back();
	assert_eq(vfs2.key, r);
	assert_eq(vfs2.adr, n);
	}
