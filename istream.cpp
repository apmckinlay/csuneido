// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "istream.h"

Istream& Istream::getline(char* s, int n) {
	if (!s || !n)
		return *this;
	bool read = false; //
	for (;;) {
		if (--n <= 0) {
			failbit = true;
			break;
		}
		int c = get();
		if (c == EOFVAL)
			break;
		read = true;
		if (c == '\n')
			break;
		*s++ = c;
	}
	if (!read)
		failbit = true;
	*s = 0;
	return *this;
}

int Istream::get() {
	if (next != -1) {
		int c = next;
		next = -1;
		return c;
	}
	int c = get_();
	if (c == EOFVAL)
		eofbit = true;
	return c;
}

Istream& Istream::read(char* buf, int n) {
	gcnt = 0;
	if (next != -1) {
		*buf++ = next;
		next = -1;
		gcnt = 1;
	}
	gcnt += read_(buf, n - gcnt);
	if (gcnt < n)
		eofbit = true;
	return *this;
}
