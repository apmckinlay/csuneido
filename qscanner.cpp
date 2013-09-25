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

#include "qscanner.h"
#include <string.h>
#include <algorithm>
#include "cmpic.h"

int QueryScanner::next()
	{
	int token;
	do
		token = nextall();
		while (token == T_WHITE ||
			token == T_NEWLINE ||
			token == T_COMMENT);
	return token;
	}

struct Keyword
	{
	char* word;
	int id;
	};
static Keyword words[] =
	{
	{ "alter", K_ALTER }, { "and", T_AND },
	{ "average", K_AVERAGE }, { "by", K_BY }, { "cascade", K_CASCADE },
	{ "count", K_COUNT }, { "create", K_CREATE }, { "delete", K_DELETE }, 
	{ "destroy", K_DROP }, { "drop", K_DROP },
	{ "ensure", K_ENSURE }, { "extend", K_EXTEND }, 
	{ "false", K_FALSE }, { "history", K_HISTORY }, { "in", K_IN },
	{ "index", K_INDEX }, { "insert", K_INSERT }, 
	{ "intersect", K_INTERSECT }, { "into", K_INTO }, 
	{ "is", I_IS }, { "isnt", I_ISNT },
	{ "join", K_JOIN }, { "key", K_KEY },
	{ "leftjoin", K_LEFTJOIN }, { "list", K_LIST },
	{ "max", K_MAX }, { "min", K_MIN }, { "minus", K_MINUS }, 
	{ "not", I_NOT }, { "or", T_OR }, { "project", K_PROJECT },
	{ "remove", K_REMOVE }, { "rename", K_RENAME }, { "reverse", K_REVERSE },
	{ "set", K_SET }, { "sort", K_SORT },
	{ "summarize", K_SUMMARIZE }, { "sview", K_SVIEW }, 
	{ "times", K_TIMES }, { "to", K_TO },
	{ "total", K_TOTAL }, { "true", K_TRUE }, { "union", K_UNION },
	{ "unique", K_UNIQUE },
	{ "update", K_UPDATE }, { "view", K_VIEW }, { "where", K_WHERE }
	};
const int nwords = sizeof words / sizeof (Keyword);
struct QscannerLt
	{
	bool operator()(const Keyword& k1, const Keyword& k2)
		{ return strcmpic(k1.word, k2.word) < 0; }
	bool operator()(const Keyword& k, char* s)
		{ return strcmpic(k.word, s) < 0; }
	bool operator()(char* s, const Keyword& k)
		{ return strcmpic(s, k.word) < 0; }
	};

int QueryScanner::keywords(char* s)
	{
	std::pair<Keyword*,Keyword*> p = std::equal_range(words, words + nwords, s, QscannerLt());
	return p.first < p.second ? p.first->id : 0;
	}

void QueryScanner::insert(const gcstring& s)
	{
	source = (s + (source + si)).str();
	si = 0;
	prev = 0;
	value = "";
	err = "";
	}

#include "testing.h"
#include "except.h"

static char* qscanner_input = "alter average by count create delete \
		destroy drop ensure extend false history in into index insert intersect join \
		key leftjoin list max min minus project remove rename reverse set \
		sort summarize times to total true union unique update view where";

static short results[] =
	{
	K_ALTER, K_AVERAGE, K_BY,
	K_COUNT, K_CREATE, K_DELETE, 
	K_DROP, K_DROP, K_ENSURE, K_EXTEND, K_FALSE,
	K_HISTORY, K_IN, K_INTO, K_INDEX, K_INSERT, 
	K_INTERSECT, K_JOIN, K_KEY, K_LEFTJOIN, K_LIST,
	K_MAX, K_MIN, K_MINUS, K_PROJECT, K_REMOVE,
	K_RENAME, K_REVERSE, K_SET, K_SORT, K_SUMMARIZE,
	K_TIMES, K_TO, K_TOTAL, K_TRUE, K_UNION,
	K_UNIQUE, K_UPDATE, K_VIEW, K_WHERE, 
	};

class test_qscanner : public Tests
	{
	TEST(0, qscanner)
		{
		short token;
		QueryScanner sc(qscanner_input);
		for (int i = 0; Eof != (token = sc.next()); ++i)
			asserteq(results[i], sc.keyword);
		}
	};
REGISTER(test_qscanner);
