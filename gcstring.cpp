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

#include "gcstring.h"
#include "std.h"
#include "except.h"
#include "minmax.h"
#include "ctype.h"
#include "gc.h"

// defer concatenation by storing left & right parts separately
struct Concat
	{
	Concat(const gcstring& x, const gcstring& y) : left(x), right(y)
		{ }
	gcstring left;
	gcstring right;
	};

// always allocate an extra character for a nul
// this ensures (even for substr's) that p[n] is always a valid address

gcstring::gcstring(size_t nn) : n(nn), p(nn == 0 ? empty_buf : new(noptrs) char[nn + 1])
	{
	if (n != 0)
		p[n] = 0;
	}

char* gcstring::empty_buf = "";

void gcstring::init(const char* p2, size_t n2)
	{
	if (n2 == 0)
		{
		n = 0;
		p = empty_buf;
		verify(*empty_buf == 0); // make sure it hasn't been corrupted
		return ;
		}
	verify(p2);
	// note: always nul terminated
	p = new(noptrs) char[n2 + 1];
	n = n2;
	verify(n >= 0);
	memcpy((void*) p, (void*) p2, n2);
	p[n2] = 0;
	}

gcstring& gcstring::operator+=(const gcstring& s)
	{
	const int LARGE = 64;
	const int totsize = size() + s.size();

	if (s.size() == 0)
		;
	else if (size() == 0)
		*this = s;
	else if (totsize > LARGE)
		{
		cc = new Concat(*this, s);
		n = -totsize;
		}
	else
		{
		char* q = new(noptrs) char[totsize + 1];
		memcpy(q, buf(), size());
		memcpy(q + size(), s.buf(), s.size());
		n += s.size();
		q[n] = 0;
		p = q;
		}
	return *this;
	}

const char* gcstring::str() const
	{
	if (n < 0)
		flatten();
	else if (p[n] != 0) // caused by substr
		{
		verify(n != 0);
		char* q = new(noptrs) char[n + 1];
		memcpy((void*) q, (void*) p, n);
		q[n] = 0;
		p = q;
		}
	//verify(! gc_inheap(p) || n < gc_size(p));
	return p;
	}

char* gcstring::str()
	{
	if (n < 0)
		flatten();
	else if (p[n] != 0) // caused by substr
		{
		verify(n != 0);
		char* q = new(noptrs) char[n + 1];
		memcpy((void*) q, (void*) p, n);
		q[n] = 0;
		p = q;
		}
	return p;
	}

bool operator<(const gcstring& x, const gcstring& y)
	{
	size_t minlen = x.size() < y.size() ? x.size() : y.size();
	int r = memcmp(x.buf(), y.buf(), minlen);
	if (r == 0)
		r = x.size() - y.size();
	return r < 0;
	}

gcstring gcstring::substr(size_t i, int len) const
	{
	if (len == 0)
		return gcstring();
	ckflat();
	if (i > n)
		i = n;
	if (len == -1 || len > n - i)
		len = max(0, (int)(n - i));
	return gcstring(len, p + i); // no alloc
	}

gcstring gcstring::trim() const
	{
	ckflat();
	int i = 0;
	while (i < n && isspace(p[i]))
		++i;
	int j = n - 1;
	while (j > i && isspace(p[j]))
		--j;
	return substr(i, j - i + 1);
	}

int gcstring::find(char c, size_t pos) const
	{
	if (pos >= size())
		return -1;
	ckflat();
	char* q = (char*) memchr(p + pos, c, n - pos);
	return q ? q - p : -1;
	}

int gcstring::find(const gcstring& x, size_t pos) const
	{
	ckflat();
	x.ckflat();
	int lim = n - x.n;
	for (int i = max(pos, 0u); i <= lim; ++i)
		if (0 == memcmp(p + i, x.p, x.n))
			return i;
	return -1;
	}

int gcstring::findlast(const gcstring& x, size_t pos) const
	{
	if (x.size() > size())
		return -1;
	ckflat();
	x.ckflat();
	for (int i = min(pos, (size_t) n - x.n); i >= 0; --i)
		if (0 == memcmp(p + i, x.p, x.n))
			return i;
	return -1;
	}

bool gcstring::has_prefix(const gcstring& x, size_t pos) const
	{
	if (pos + x.size() > size())
		return false;
	ckflat();
	x.ckflat();
	return 0 == memcmp(p + pos, x.p, x.n);
	}

bool gcstring::has_suffix(const gcstring& x) const
	{
	if (x.size() > size())
		return false;
	ckflat();
	x.ckflat();
	return 0 == memcmp(p + n - x.n, x.p, x.n);
	}

bool has_prefix(const char* s, const char* pre)
	{
	return 0 == memcmp(s, pre, strlen(pre));
	}

bool has_suffix(const char* s, const char* suf)
	{
	size_t n_s = strlen(s);
	size_t n_suf = strlen(suf);
	return n_s >= n_suf &&
		0 == memcmp(s + n_s - n_suf, suf, n_suf);
	}

void gcstring::flatten() const
	{
	verify(n < 0);
	verify(cc->left.size() + cc->right.size() == -n);
	char* s = new(noptrs) char[-n + 1];
	copy(s, this);
	n = -n;
	s[n] = 0;
	p = s;
	}

// copy a string, eliminating Concats
// recurses on right side, iterates on left
// on the assumption that you mostly concatenate onto the right
void gcstring::copy(char* dst, const gcstring* s)
	{
	while (s->n < 0)
		{
		// copy right side
		if (s->cc->right.n >= 0)
			memcpy(dst + s->cc->left.size(), s->cc->right.p, s->cc->right.n);
		else
			copy(dst + s->cc->left.size(), &s->cc->right);

		// iterate on left side
		s = &s->cc->left;
		}
	memcpy(dst, s->p, s->n);
	}

Ostream& operator<<(Ostream& os, const gcstring& s)
	{
	(void) os.write(s.buf(), s.size());
	return os;
	}

gcstring gcstring::to_heap()
	{
	if (! gc_inheap(buf()))
		{
		char* q = new(noptrs) char[n + 1];
		memcpy((void*) q, (void*) p, n);
		q[n] = 0;
		p = q;
		}
	return *this;
	}

#include "testing.h"

class test_gcstring : public Tests
	{
	TEST(1, construct)
		{
		gcstring s;
		verify(s == "");
		gcstring t("hello");
		verify(t == "hello");
		}
	TEST(2, compare)
		{
		gcstring s = "hello world";
		verify("a" < s);
		gcstring t("hello");
		verify(t < s);
		}
	TEST(3, substr)
		{
		gcstring s = "hello world";
		verify(s.substr(0) == s);
		verify(s.substr(6) == "world");
		verify(s.substr(3,5) == "lo wo");
		}
	TEST(4, concat)
		{
		gcstring s = "hello world";
		gcstring t("hello");
		verify(t + " world" == s);
		gcstring x = "12";
		x += "34";
		gcstring y = "56";
		y += "78";
		verify(x + y == "12345678");

		s = "";
		const int N = 10;
		char* big = "now is the time for all good men to come to the aid of their party.";
		int bigsize = strlen(big);
		int i;
		for (i = 0; i < N; ++i)
			s += big;
		asserteq(s.size(), N * bigsize);
		for (i = 0; i < N; ++i)
			verify(has_prefix(s.buf() + i * bigsize, big));
		}
	TEST(5, find)
		{
		gcstring s = "hello world";
		verify(s.find("lo") == 3);
		verify(s.find("x") == -1);
		verify(s.find("o", 5) == 7);

		s = "01234567890123456789012345678901234567890123456789012345678901234567890123456789";
		gcstring t = "0123456789012345678901234567890123456789";
		t += "0123456789012345678901234567890123456789";
		asserteq(s.find(t), 0);
		}
	TEST(6, presuffix)
		{
		gcstring s = "hello world";
		verify(s.has_prefix(""));
		verify(s.has_suffix(""));
		verify(s.has_prefix("hello"));
		verify(s.has_suffix("world"));
		verify(! s.has_suffix("hello"));
		verify(! s.has_prefix("world"));
		}
	TEST(7, trim)
		{
		gcstring hello("hello");
		asserteq(hello.trim(), hello);
		asserteq(gcstring("  hello").trim(), hello);
		asserteq(gcstring("hello  ").trim(), hello);
		asserteq(gcstring(" hello ").trim(), hello);
		}
	};
REGISTER(test_gcstring);
