#pragma once
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

#include "scanner.h"

class gcstring;

class QueryScanner : public Scanner
	{
public:
	explicit QueryScanner(const char* buf) : Scanner(buf)
		{ }
	int next() override;
	void insert(const gcstring& s);
protected:
	int keywords(const char*) override;
	};

enum { K_ALTER = NEXT_KEYWORD, K_AVERAGE, K_BY, K_CASCADE, K_COUNT, K_CREATE,
	K_DELETE, K_DROP, K_ENSURE, K_EXTEND, K_HISTORY, K_INDEX,
	K_INSERT, K_INTERSECT, K_INTO, K_JOIN, K_KEY, K_LEFTJOIN, K_LIST, K_LOWER,
	K_MAX, K_MIN, K_MINUS, K_PROJECT, K_REMOVE, K_RENAME, K_REVERSE,
	K_SET, K_SORT, K_SUMMARIZE, K_SVIEW, K_TIMES, K_TO, K_TOTAL, K_UNION,
	K_UNIQUE, K_UPDATE, K_VIEW, K_WHERE };
