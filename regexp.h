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

#include "gcstring.h"

// specifies a part of a string matched by a part of a regular expression
struct Rxpart
	{
	const char* s;
	int n;
	const char* tmp; // used to store tentative "s"
	};

enum { MAXPARTS = 10 };

class gcstring;

// compile a regular expression
char* rx_compile(const gcstring& s);

// match a string against a compiled regular expression
bool rx_match(const char* s, int n, int i, const char* pat, Rxpart* psubs = 0);
inline bool rx_match(const gcstring& s, const char* pat, Rxpart* psubs = 0)
	{ return rx_match(s.buf(), s.size(), 0, pat, psubs); }
bool rx_match_reverse(const char* s, int n, int i, const char* pat, Rxpart* psubs = 0);

// match a specific point in a string against a compiled regular expression
int rx_amatch(const char* s, int i, int n, const char* pat, Rxpart* psubs = 0);

// determine the length of a replacement string
int rx_replen(const char* rep, Rxpart* subs);

// build a replacement string
char* rx_mkrep(char* buf, const char* rep, Rxpart* subs);
