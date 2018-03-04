#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2017 Suneido Software Corp.
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

class SuValue;

/*
 * Used by SuObject and SuInstance to handle recursive references
 * NOTE: Static so not thread safe. Must not yield within compare.
 */
struct EqNest
	{
	EqNest(const SuValue* x, const SuValue* y)
		{
		if (n >= MAX)
			except("object comparison nesting overflow");
		xs[n] = x;
		ys[n] = y;
		++n;
		}
	static bool has(const SuValue* x, const SuValue* y)
		{
		for (int i = 0; i < n; ++i)
			if ((xs[i] == x && ys[i] == y) || (xs[i] == y && ys[i] == x))
				return true;
		return false;
		}
	~EqNest()
		{
		--n; verify(n >= 0);
		}
	enum { MAX = 20 };
	static int n;
	static const SuValue* xs[MAX];
	static const SuValue* ys[MAX];
	// definitions in suobject.cpp
	// TODO use inline when supported
	};
