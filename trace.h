#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

extern int trace_level;

enum {
	TRACE_FUNCTIONS = 1 << 0,
	TRACE_STATEMENTS = 1 << 1,
	TRACE_OPCODES = 1 << 2,
	TRACE_RECORDS = 1 << 3,
	TRACE_LIBRARIES = 1 << 4,
	TRACE_SLOWQUERY = 1 << 5,
	TRACE_QUERY = 1 << 6,
	TRACE_SYMBOL = 1 << 7,
	TRACE_ALLINDEX = 1 << 8,
	TRACE_TABLE = 1 << 9,
	TRACE_SELECT = 1 << 10,
	TRACE_TEMPINDEX = 1 << 11,
	TRACE_QUERYOPT = 1 << 12,

	TRACE_CONSOLE = 1 << 13,
	TRACE_LOGFILE = 1 << 14,

	TRACE_CLIENTSERVER = 1 << 15,
	TRACE_EXCEPTIONS = 1 << 16,
	TRACE_GLOBALS = 1 << 17,

	TRACE_JOINOPT = 1 << 18,
};

#define TRACE_CLEAR() trace_level &= ~15

class Ostream;

extern Ostream& tout();

#define TRACE(mask, stuff) \
	if ((trace_level & TRACE_##mask)) \
	tout() << #mask << ' ' << stuff << endl // NOLINT(misc-macro-parentheses)
