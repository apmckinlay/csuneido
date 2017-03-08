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
#include "ostreamstr.h"
#include "cachemap.h"

using namespace std;

static gcstring makset(const gcstring&);
static gcstring expandRanges(const gcstring&);
static int xindex(const gcstring&, char, bool, int);

gcstring tr(const gcstring& srcstr, gcstring from, const gcstring& to)
	{
	const int srclen = srcstr.size();
	if (srclen == 0 || from.size() == 0)
		return srcstr;

	bool allbut = (from[0] == '^');
	if (allbut)
		from = from.substr(1);
	gcstring fromset = makset(from);

	const char* src = srcstr.buf();
	int si = 0;
	for (; si < srclen; ++si)
		{
		int p = fromset.find(src[si]);
		if (allbut == (p == -1))
			break ;
		}
	if (si == srclen)
		return srcstr; // no changes

	gcstring toset = makset(to);
	int lastto = toset.size();
	bool collapse = lastto > 0 && (allbut || lastto < fromset.size());
	--lastto;

	char* buf = new char[srclen + 1];
	memcpy(buf, src, si);
	char* dst = buf + si;

	for (; si < srclen; ++si)
		{
		int i = xindex(fromset, src[si], allbut, lastto);
		// mingw g++ 5.3 gives spurious strict-overflow warning
		// if the following two lines are combined with &&
		if (collapse)
			if (i >= lastto)
				{
				*dst++ = toset[lastto];
				do
					{
					if (++si >= srclen)
						goto finished;
					i = xindex(fromset, src[si], allbut, lastto);
					}
					while (i >= lastto);
				}
		if (i < 0)
			*dst++ = src[si];
		else if (lastto >= 0)
			*dst++ = toset[i];
		/* else
			delete */
		}
finished:
	*dst = 0;
	return gcstring(dst - buf, buf); // no alloc
	}

static gcstring makset(const gcstring& s)
	{
	int dash = s.find('-', 1);
	if (dash == -1 || dash == s.size() - 1)
		return s; // no ranges to expand
	static CacheMap<10,gcstring,gcstring> cache;
	if (gcstring* p = cache.get(s))
		return *p;
	return cache.put(s, expandRanges(s));
	}

static gcstring expandRanges(const gcstring& s)
	{
	int n = s.size();
	OstreamStr dst(n);
	for (int i = 0; i < n; ++i)
		if (s[i] == '-' && i > 0 && i + 1 < n)
			for (uchar c = s[i - 1] + 1; c < (uchar) s[i + 1]; ++c)
				dst << c;
		else
			dst << s[i];
	return gcstring(dst.size(), dst.str()); // no alloc
	}

static int xindex(const gcstring& fromset, char c, bool allbut, int lastto)
	{
	int i = fromset.find(c);
	if (allbut)
		return (i == -1) ? lastto + 1 : -1;
	else
		return i;
	}

#include "testing.h"

class test_tr : public Tests
	{
	TEST(0, main)
		{
		asserteq(tr("", "", ""), "");
		asserteq(tr("", "abc", "ABC"), "");
		asserteq(tr("", "^abc", "x"), "");

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

		asserteq(tr("a b - c", "^abc", ""), "abc"); // allbut delete
		asserteq(tr("a b - c", "^a-z", ""), "abc"); // allbut delete
		asserteq(tr("a  b - c", "^abc", " "), "a b c"); // allbut collapse
		asserteq(tr("a  b - c", "^a-z", " "), "a b c"); // allbut collapse
		asserteq(tr("a-b-c", "-x", ""), "abc"); // literal dash
		asserteq(tr("a-b-c", "x-", ""), "abc"); // literal dash

		// collapse at end
		asserteq(tr("hello \t\n\n", " \t\n", "\n"), "hello\n");

		// signed char range
		asserteq(tr("hello", "^\x20-\xff", ""), "hello");
		asserteq(tr("hello\x7f", "\x70-\x7f", ""), "hello");
		asserteq(tr("hello\xff", "\x7f-\xff", ""), "hello");

		asserteq(tr("abc", "abcdefghijklmnop", "abcdefg"), "abc");
		asserteq(tr("nop", "abcdefghijklmnop", "abcdefg"), "g");
		}
	};
REGISTER(test_tr);

