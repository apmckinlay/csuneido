// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "itostr.h"
#include <algorithm>

char* utostr(unsigned int x, char* buf, int radix) {
	char* dst = buf;
	do
		*dst++ = "0123456789abcdef"[x % radix];
	while ((x /= radix) > 0);
	*dst = 0;
	std::reverse(buf, dst);
	return buf;
}

char* itostr(int x, char* buf, int radix) {
	char* dst = buf;
	if (x < 0) {
		*dst++ = '-';
		x = -x;
	}
	utostr(x, dst, radix);
	return buf;
}

char* u64tostr(uint64_t x, char* buf, int radix) {
	char* dst = buf;
	do
		*dst++ = "0123456789abcdef"[x % radix];
	while ((x /= radix) > 0);
	*dst = 0;
	std::reverse(buf, dst);
	return buf;
}

char* i64tostr(int64_t x, char* buf, int radix) {
	char* dst = buf;
	if (x < 0) {
		*dst++ = '-';
		x = -x;
	}
	u64tostr(x, dst, radix);
	return buf;
}

#include "testing.h"
#include <cstring>

TEST(itostr_unsigned) {
	char buf[20];
	verify(0 == strcmp(utostr(0, buf), "0"));
	verify(0 == strcmp(utostr(1, buf), "1"));
	verify(0 == strcmp(utostr(1234, buf), "1234"));
	verify(0 == strcmp(utostr(1024, buf, 16), "400"));
	verify(0 == strcmp(utostr(7, buf, 2), "111"));
}

TEST(itostr) {
	char buf[20];
	verify(0 == strcmp(itostr(0, buf), "0"));
	verify(0 == strcmp(itostr(1, buf), "1"));
	verify(0 == strcmp(itostr(1234, buf), "1234"));
	verify(0 == strcmp(itostr(-1, buf), "-1"));
	verify(0 == strcmp(itostr(-1234, buf), "-1234"));
}
