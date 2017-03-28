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

// stores n in p, p must have at least 4 bytes
char* cvt_long(char* p, long n);

// stores n in s, s must have at least 4 bytes
inline gcstring cvt_long(const gcstring& s, long n)
	{ cvt_long(s.buf(), n); return s; }

// p must have at least 4 bytes
long cvt_long(const char* p);

// s must have at least 4 bytes
inline long cvt_long(const gcstring& s)
	{ return cvt_long(s.buf()); }

// stores n in p, p must have at least 2 bytes
char* cvt_short(char* p, short n);

// p must have at least 2 bytes
short cvt_short(const char* p);
