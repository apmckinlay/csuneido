#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2006 Suneido Software Corp.
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
#include "except.h"

typedef int64 Mmoffset;

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
		Mmoffset off = static_cast<int64>(offset) << MM_SHIFT;
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
