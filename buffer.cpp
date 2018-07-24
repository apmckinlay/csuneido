// Copyright (c) 2013 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "buffer.h"
#include "gc.h"
#include "except.h"
#include "gcstring.h"
#include "fatal.h"
#include <cstring>
#include <algorithm>
using std::max;

Buffer::Buffer(int n)
	: buf(new (noptrs) char[n]), capacity(n), used(0), pos(0) {
}

char* Buffer::ensure(int n) {
	++n; // allow for nul for gcstr
	if (used + n > capacity) {
		buf =
			(char*) GC_realloc(buf, capacity = max(2 * capacity, capacity + n));
		if (!buf)
			fatal("out of memory");
	}
	return buf + used;
}

char* Buffer::alloc(int n) {
	verify(n >= 0);
	char* dst = ensure(n);
	added(n);
	return dst;
}

Buffer& Buffer::add(char c) {
	*alloc(1) = c;
	return *this;
}

Buffer& Buffer::add(const char* s, int n) {
	if (n > 0)
		memcpy(alloc(n), s, n);
	return *this;
}

Buffer& Buffer::add(const gcstring& s) {
	int n = s.size();
	memcpy(alloc(n), s.ptr(), n);
	return *this;
}

void Buffer::remove(int n) {
	verify(n <= used);
	memmove(buf, buf + n, used - n);
	used -= n;
}

gcstring Buffer::gcstr() const {
	return gcstring::noalloc(str(), used);
}

char* Buffer::str() const {
	verify(used < capacity);
	buf[used] = 0;
	return buf;
}

gcstring Buffer::getStr(int n) {
	verify(pos + n <= used);
	gcstring s(buf + pos, n);
	pos += n;
	return s;
}

char* Buffer::getBuf(int n) {
	verify(pos + n <= used);
	int i = pos;
	pos += n;
	return buf + i;
}
