#pragma once
// Copyright (c) 2015 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

const int MAX_LINE = 4000;

// Consumes an entire line (up to newline or eof)
// but only returns the first MAX_LINE characters
// Returned line does not include \r or \n
// Used by sufile and runpiped
#define READLINE(getc) \
	char buf[MAX_LINE + 1]; \
	int i = 0; \
	while (getc) { \
		if (i < MAX_LINE) \
			buf[i++] = c; \
		if (c == '\n') \
			break; \
	} \
	if (i == 0) \
		return SuFalse; \
	while (i > 0 && (buf[i - 1] == '\r' || buf[i - 1] == '\n')) \
		--i; \
	buf[i] = 0; \
	return new SuString(buf, i)
