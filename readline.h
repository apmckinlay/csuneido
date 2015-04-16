#ifndef READLINE_H
#define READLINE_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2015 Suneido Software Corp. 
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

const int MAX_LINE = 4000;

// Consumes an entire line (up to newline or eof)
// but only returns the first MAX_LINE characters
// Returned line does not include \r or \n
// Used by sufile and runpiped
#define READLINE(getc) \
	char buf[MAX_LINE + 1]; \
	int i = 0; \
	while (getc) \
		{ \
		if (i < MAX_LINE) \
			buf[i++] = c; \
		if (c == '\n') \
			break; \
		} \
	if (i == 0) \
		return SuFalse; \
	while (i > 0 && (buf[i-1] == '\r' || buf[i-1] == '\n')) \
		--i; \
	buf[i] = 0; \
	return new SuString(buf, i)

#endif