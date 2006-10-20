#ifndef REGEXP_H
#define REGEXP_H

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

// specifies a part of a string matched by a part of a regular expression
struct Rxpart
	{
	char* s;
	int n;
	char* tmp; // used to store tentative "s"
	};

enum { MAXPARTS = 10 };

class gcstring;

// compile a regular expression
char* rx_compile(const gcstring& s);

// match a string against a compiled regular expression
bool rx_match(char* s, int n, char* pat, Rxpart* psubs = 0);

// match a specific point in a string against a compiled regular expression
int rx_amatch(char* s, int i, int n, char* pat, Rxpart* psubs = 0);

// determine the length of a replacement string
int rx_replen(const char* rep, Rxpart* subs);

// build a replacement string
char* rx_mkrep(char* buf, const char* rep, Rxpart* subs);

#endif
