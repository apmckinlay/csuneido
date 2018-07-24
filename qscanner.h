#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "scanner.h"

class gcstring;

class QueryScanner : public Scanner {
public:
	explicit QueryScanner(const char* buf) : Scanner(buf) {
	}
	int next() override;
	void insert(const gcstring& s);

protected:
	int keywords(const char*) override;
};

enum {
	K_ALTER = NEXT_KEYWORD,
	K_AVERAGE,
	K_BY,
	K_CASCADE,
	K_COUNT,
	K_CREATE,
	K_DELETE,
	K_DROP,
	K_ENSURE,
	K_EXTEND,
	K_HISTORY,
	K_INDEX,
	K_INSERT,
	K_INTERSECT,
	K_INTO,
	K_JOIN,
	K_KEY,
	K_LEFTJOIN,
	K_LIST,
	K_LOWER,
	K_MAX,
	K_MIN,
	K_MINUS,
	K_PROJECT,
	K_REMOVE,
	K_RENAME,
	K_REVERSE,
	K_SET,
	K_SORT,
	K_SUMMARIZE,
	K_SVIEW,
	K_TIMES,
	K_TO,
	K_TOTAL,
	K_UNION,
	K_UNIQUE,
	K_UPDATE,
	K_VIEW,
	K_WHERE
};
