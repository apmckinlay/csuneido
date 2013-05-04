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

#include "tr.h"
#include "except.h"
#include <string.h>
#include <vector>
#include <algorithm>

using namespace std;

static vector<char>  makset(const gcstring&);
static int xindex(const vector<char>&, char, bool, int);

#define COPY() \
	do { \
	if (! buf) \
		dst = buf = new char[srclen + 1]; \
	if (nsame > 0) \
		{ \
		memcpy(dst, src + si - nsame, nsame); \
		dst += nsame; \
		nsame = 0; \
		} \
	} while (false)

#define ADDCHAR(c) \
	do { \
	COPY(); \
	*dst++ = c; \
	} while (false)

gcstring tr(const gcstring& srcstr, gcstring from, const gcstring& to)
	{
	bool allbut = (from[0] == '^');
	if (allbut)
		from = from.substr(1);
	vector<char> fromset = makset(from);
	vector<char> toset = makset(to);

	int lastto = toset.size();
	bool collapse = allbut || lastto < fromset.size();
	--lastto;

	const int srclen = srcstr.size();
	const char* src = srcstr.buf();
	char* buf = 0;
	char* dst = 0;
	int nsame = 0;
	int si = 0;
	for (; si < srclen; ++si)
		{
		int i = xindex(fromset, src[si], allbut, lastto);
		if (collapse && 0 <= lastto && lastto <= i)
			{
			ADDCHAR(toset[lastto]);
			do
				i = xindex(fromset, src[++si], allbut, lastto);
				while (i >= lastto);
			}
		if (si >= srclen)
			break ;
		if (i >= 0 && lastto >= 0)
			ADDCHAR(toset[i]);
		else if (i < 0)
			++nsame; // defer copying 
		else // delete
			COPY();
		}
	if (buf == 0)
		return srcstr; // no changes
	ADDCHAR(0);
	return gcstring(dst - buf - 1, buf);
	}

static vector<char> makset(const gcstring& src)
	{
	vector<char> dst;
	for (int i = 0; i < src.size(); ++i)
		{
		if (src[i] == '-' && i > 0 && i + 1 < src.size())
			{
			for (char c = src[i - 1] + 1; c < src[i + 1]; ++c)
				dst.push_back(c);
			}
		else
			dst.push_back(src[i]);
		}
	return dst;
	}

static int xindex(const vector<char>& from, char c, bool allbut, int lastto)
	{
	int p = find(from.begin(), from.end(), c) - from.begin();
	int i;
	if (allbut && p != from.size())
		i = -1;
	else if (allbut && p == from.size())
		i = lastto + 1;
	else if (p != from.size())
		i = p;
	else
		i = -1;
	return i;
	}

#include "testing.h"

class test_tr : public Tests
	{
	TEST(0, main)
		{
		asserteq(tr("", "", ""), "");
		asserteq(tr("abc", "", ""), "abc");
		asserteq(tr("abc", "xyz", ""), "abc");
		asserteq(tr("zon", "xyz", ""), "on");
		asserteq(tr("oyn", "xyz", ""), "on");
		asserteq(tr("nox", "xyz", ""), "no");
		asserteq(tr("zyx", "xyz", ""), "");

		asserteq(tr("zon", "xyz", "XYZ"), "Zon");
		asserteq(tr("oyn", "xyz", "XYZ"), "oYn");
		asserteq(tr("nox", "xyz", "XYZ"), "noX");
		asserteq(tr("zyx", "xyz", "XYZ"), "ZYX");
		}
	};
REGISTER(test_tr);

