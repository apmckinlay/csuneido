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

#include "range.h"
#include "prim.h"
#include "interp.h"
#include "suobject.h"
#include <limits.h>

class RangeTo : public Range
	{
public:
	RangeTo(int f, int t) : from(f), to(t)
		{ }
	gcstring substr(gcstring s) override;
	SuObject* sublist(SuObject* ob) override;
	void out(Ostream& os) override
		{ os << "Range(" << from << " .. " << to << ")"; }
private:
	int from;
	int to;
	};

class RangeLen : public Range
	{
public:
	RangeLen(int f, int n) : from(f), len(n)
		{ }
	gcstring substr(gcstring s) override;
	SuObject* sublist(SuObject* ob) override;
	void out(Ostream& os) override
		{ os << "Range(" << from << " :: " << len << ")"; }
private:
	int from;
	int len;
	};

Value rangeTo()
	{
	const int nargs = 2;
	int from = ARG(0).integer();
	int to = ARG(1) == SuFalse ? INT_MAX : ARG(1).integer();
	return new RangeTo(from, to);
	}
PRIM(rangeTo, "RangeTo(from, to)");

Value rangeLen()
	{
	const int nargs = 2;
	int from = ARG(0).integer();
	int len = ARG(1) == SuFalse ? INT_MAX : ARG(1).integer();
	if (len < 0)
		len = 0;
	return new RangeLen(from, len);
	}
PRIM(rangeLen, "RangeLen(from, len)");

static int prepFrom(int from, int len)
	{
	if (from < 0)
		{
		from += len;
		if (from < 0)
			from = 0;
		}
	return from;
	}

static int prepTo(int to, int len)
	{
	if (to < 0)
		to += len;
	if (to > len)
		to = len;
	return to;
	}

gcstring RangeTo::substr(gcstring s)
	{
	int size = s.size();
	int f = prepFrom(from, size);
	int t = prepTo(to, size);
	if (t <= f)
		return "";
	return s.substr(f, t - f);
	}

gcstring RangeLen::substr(gcstring s)
	{
	int size = s.size();
	int f = prepFrom(from, size);
	return s.substr(f, len);
	}

static SuObject* sublist(SuObject* ob, int from, int to)
	{
	int size = ob->vecsize();
	SuObject* result = new SuObject();
	for (int i = from; i < to && i < size; ++i)
		result->add(ob->get(i));
	return result;
	}

SuObject* RangeTo::sublist(SuObject* ob)
	{
	int size = ob->vecsize();
	int f = prepFrom(from, size);
	int t = prepTo(to, size);
	return ::sublist(ob, f, t);
	}

SuObject* RangeLen::sublist(SuObject* ob)
	{
	int size = ob->vecsize();
	int f = prepFrom(from, size);
	if (len > size - f)
		len = size - f;
	int t = f + len;
	return ::sublist(ob, f, t);
	}
