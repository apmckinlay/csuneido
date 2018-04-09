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
#include "cachemap.h"
#include "buffer.h"

using namespace std;

static gcstring makset(const gcstring&);
static gcstring expandRanges(const gcstring&);
static int xindex(const gcstring&, char, bool, int);

gcstring tr(const gcstring& srcstr, const gcstring& from, const gcstring& to)
	{
	const int srclen = srcstr.size();
	if (srclen == 0 || from.size() == 0)
		return srcstr;

	bool allbut = (from[0] == '^');
	gcstring fromset = makset(allbut ? from.substr(1) : from);

	auto src = srcstr.ptr();
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

	char* buf = salloc(srclen);
	memcpy(buf, src, si);
	char* dst = buf + si;

	for (; si < srclen; ++si)
		{
		int i = xindex(fromset, src[si], allbut, lastto);
		if (collapse && i >= lastto)
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
	return gcstring::noalloc(buf, dst - buf);
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
	Buffer dst(n);
	for (int i = 0; i < n; ++i)
		if (s[i] == '-' && i > 0 && i + 1 < n)
			for (uchar c = s[i - 1] + 1; c < (uchar) s[i + 1]; ++c)
				dst .add(c);
		else
			dst.add(s[i]);
	return dst.gcstr();
	}

static int xindex(const gcstring& fromset, char c, bool allbut, int lastto)
	{
	int i = fromset.find(c);
	if (allbut)
		return (i == -1) ? lastto + 1 : -1;
	else
		return i;
	}

// tests ------------------------------------------------------------

#include "testing.h"

TEST(tr)
	{
	assert_eq(tr("", "", ""), "");
	assert_eq(tr("", "abc", "ABC"), "");
	assert_eq(tr("", "^abc", "x"), "");

	assert_eq(tr("abc", "", ""), "abc");
	assert_eq(tr("abc", "xyz", ""), "abc");
	assert_eq(tr("zon", "xyz", ""), "on");
	assert_eq(tr("oyn", "xyz", ""), "on");
	assert_eq(tr("nox", "xyz", ""), "no");
	assert_eq(tr("zyx", "xyz", ""), "");

	assert_eq(tr("zon", "xyz", "XYZ"), "Zon");
	assert_eq(tr("oyn", "xyz", "XYZ"), "oYn");
	assert_eq(tr("nox", "xyz", "XYZ"), "noX");
	assert_eq(tr("zyx", "xyz", "XYZ"), "ZYX");

	assert_eq(tr("a b - c", "^abc", ""), "abc"); // allbut delete
	assert_eq(tr("a b - c", "^a-z", ""), "abc"); // allbut delete
	assert_eq(tr("a  b - c", "^abc", " "), "a b c"); // allbut collapse
	assert_eq(tr("a  b - c", "^a-z", " "), "a b c"); // allbut collapse
	assert_eq(tr("a-b-c", "-x", ""), "abc"); // literal dash
	assert_eq(tr("a-b-c", "x-", ""), "abc"); // literal dash

	// collapse at end
	assert_eq(tr("hello \t\n\n", " \t\n", "\n"), "hello\n");

	// signed char range
	assert_eq(tr("hello", "^\x20-\xff", ""), "hello");
	assert_eq(tr("hello\x7f", "\x70-\x7f", ""), "hello");
	assert_eq(tr("hello\xff", "\x7f-\xff", ""), "hello");

	assert_eq(tr("abc", "abcdefghijklmnop", "abcdefg"), "abc");
	assert_eq(tr("nop", "abcdefghijklmnop", "abcdefg"), "g");
	}

TEST(tr_expandRanges)
	{
	gcstring s = expandRanges(gcstring("\x00-\xff", 3));
	assert_eq(s.size(), 256);
	assert_eq(s[0], '\x00');
	assert_eq(s[255], '\xff');
	}

