#ifndef TRACE_H
#define TRACE_H

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

extern int trace_level;

enum 
	{
	TRACE_FUNCTIONS =	1 << 0,
	TRACE_STATEMENTS =	1 << 1,
	TRACE_OPCODES =	1 << 2,
	TRACE_STACK =		1 << 3,
	TRACE_LIBLOAD =	1 << 4,
	TRACE_SLOWQUERY =	1 << 5,
	TRACE_QUERY =		1 << 6,
	TRACE_SYMBOL =	1 << 7,
	TRACE_ALLINDEX =	1 << 8,
	TRACE_TABLE =		1 << 9,
	TRACE_SELECT =		1 << 10,
	TRACE_TEMPINDEX =	1 << 11,
	TRACE_QUERYOPT =	1 << 12,
	
	TRACE_CONSOLE =	1 << 13,
	TRACE_LOGFILE =	1 << 14
	};

#define TRACE_CLEAR() trace_level &= ~15

class Ostream;

extern Ostream& tout();

#define TRACE(mask, stuff) \
	if ((trace_level & TRACE_##mask)) \
		tout() << #mask << ' ' << stuff << endl

#endif
