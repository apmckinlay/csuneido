#ifndef CATSTR_H
#define CATSTR_H

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

#include <malloc.h>
#include <string.h>

char* catstr(char *dst, ...);

#define CATSTRA(s, t) \
	catstr((char*) alloca(strlen(s) + strlen(t) + 1), s, t, 0)
#define CATSTR3(s1, s2, s3) \
	catstr((char*) alloca(strlen(s1) + strlen(s2) + strlen(s3) + 1), s1, s2, s3, 0)
#define STRDUPA(s) \
	strcpy((char*) alloca(strlen(s) + 1), s)
#define PREFIXA(s, n) \
	strcpyn((char*) alloca(n + 1), s, n)

inline char* strcpyn(char* dst, const char* src, int n)
	{
	memcpy(dst, src, n);
	dst[n] = 0;
	return dst;
	}

#endif
