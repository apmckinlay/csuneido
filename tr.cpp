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

inline void addchar(char*& dst, char* lim, char c)
	{
	verify(dst < lim);
	*dst++ = c; 
	}

gcstring tr(const gcstring& src, gcstring from, const gcstring& to)
	{
	bool allbut = (from[0] == '^');
	if (allbut)
		from = from.substr(1);
	vector<char> fromset = makset(from);
	vector<char> toset = makset(to);

	int lastto = toset.size();
	bool collapse = allbut || lastto < fromset.size();
	--lastto;

	const int srclen = src.size();
	char* buf = new char[srclen + 1];
	char* lim = buf + srclen;
	char* dst = buf;
	for (int si = 0; si < src.size(); ++si)
		{
		int i = xindex(fromset, src[si], allbut, lastto);
		if (collapse && 0 <= lastto && lastto <= i)
			{
			addchar(dst, lim, toset[lastto]);
			do
				i = xindex(fromset, src[++si], allbut, lastto);
				while (i >= lastto);
			}
		if (si >= src.size())
			break ;
		if (i >= 0 && lastto >= 0)
			addchar(dst, lim, toset[i]);
		else if (i < 0)
			addchar(dst, lim, src[si]);
		/* else
			delete */
		}
	*dst = 0;
	return gcstring(dst - buf, buf);
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

	
