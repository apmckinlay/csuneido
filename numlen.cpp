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

#include <ctype.h>

int numlen(const char* s)
	{
	const char* start = s;

	if (*s == '+' || *s == '-')
		++s;
	int intdigits = isdigit(*s); // really bool
	while (isdigit(*s))
		++s;
	if (*s == '.')
		++s;
	int fracdigits = isdigit(*s); // really bool
	while (isdigit(*s))
		++s;
	if (! intdigits && ! fracdigits)
		return -1;
	if ((*s == 'e' || *s == 'E') &&
		(isdigit(s[1]) || ((s[1] == '-' || s[1] == '+') && isdigit(s[2]))))
		{
		s += ((s[1] == '-' || s[1] == '+') ? 3 : 2);
		while (isdigit(*s))
			++s;
		}
	return s - start;
	}
