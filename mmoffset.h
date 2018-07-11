#pragma once
// Copyright (c) 2006 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "std.h"
#include "except.h"

typedef int64_t Mmoffset;

const int MM_SHIFT = 2;

inline int mmoffset_to_int(Mmoffset off)
	{
	return off >> MM_SHIFT;
	}

inline Mmoffset int_to_mmoffset(int n)
	{
	return static_cast<Mmoffset>(static_cast<unsigned int>(n)) << MM_SHIFT;
	}

class Mmoffset32
	{
public:
	Mmoffset32() : offset(0)
		{ }
	explicit Mmoffset32(Mmoffset o) : offset(o >> MM_SHIFT)
		{
		verify(o >= 0);
		verify(unpack() == o);
		}
	Mmoffset unpack() const
		{
		Mmoffset off = static_cast<int64_t>(offset) << MM_SHIFT;
		verify(off >= 0);
		return off;
		}
	Mmoffset32 operator=(Mmoffset o)
		{
		verify(o >= 0);
		offset = o >> MM_SHIFT;
		verify(unpack() == o); // i.e. o was aligned
		return *this;
		}
	bool operator==(Mmoffset o) const
		{ return unpack() == o; }
private:
	unsigned int offset;
	};
