/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2003 Suneido Software Corp. 
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

#include "getnum.h"
#include "except.h"
#include "gcstring.h"
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

int getnum(char type, char*& s)
	{
	while (isspace(*s))
		++s;
	if (toupper(s[0]) != type || 
		! (isdigit(s[1]) || (s[1] == '-' && isdigit(s[2]))))
		return ERR;
	++s;
	long n = strtol(s, &s, 10);
	if (n < INT_MIN || INT_MAX < n)
		return ERR;
	while (isspace(*s))
		++s;
	return n;
	}

int ck_getnum(char type, char*& s)
	{
	int num = getnum(type, s);
	if (num == ERR)
		{
		gcstring t(strlen(s), s); // no alloc (except copies)
		if (t.has_suffix("\r\n"))
			t = t.substr(0, t.size() - 2);
		except("expecting: " << type << "#, got: " << t);
		}
	return num;
	}
