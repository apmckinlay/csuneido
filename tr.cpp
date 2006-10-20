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

// TODO: eliminate or check BUFSIZE
// use gcstring, don't use nul terminators

#include "tr.h"
#include "except.h"
#include <string.h>

static void makset(char*, char*);
static int xindex(char *, char, bool, int);

const int BUFSIZE = 1024;

inline void addchar(char*& dst, char* lim, char c)
	{ verify(dst < lim); *dst++ = c; }

char* tr(char* src, char* from, char* to)
	{
	char fromset[BUFSIZE], toset[BUFSIZE];

	bool allbut = (*from == '^');
	if (allbut)
		++from;
	makset(fromset, from);

	if (to)
		makset(toset, to);
	else
		toset[0] = 0;

	int lastto = strlen(toset);
	bool collapse = allbut || lastto < (int) strlen(fromset);
	--lastto;

	const int srclen = strlen(src);
	char* buf = new char[srclen + 1];
	char* lim = buf + srclen;
	char* dst = buf;
	for (; *src; ++src)
		{
		verify(dst < lim);
		int i = xindex(fromset, *src, allbut, lastto);
		if (collapse && i >= lastto && lastto >= 0)
			{
			addchar(dst, lim, toset[lastto]);
			do
				i = xindex(fromset, *++src, allbut, lastto);
				while (i >= lastto);
			}
		if (! *src)
			break ;
		if (i >= 0 && lastto >= 0)
			addchar(dst, lim, toset[i]);
		else if (i < 0)
			addchar(dst, lim, *src);
		/* else
			delete */
		}
	*dst = 0;
	return buf;
	}

static void makset(char* dst, char* src)
	{
	for (char* start = src; *src; ++src)
		{
		if (*src == '-' && src > start && src[1])
			{
			for (char c = src[-1] + 1; c < src[1]; ++c)
				*dst++ = c;
			}
		else
			*dst++ = *src;
		}
	*dst = 0;
	}

static int xindex(char* from, char c, bool allbut, int lastto)
	{
	char* p = strchr(from, c);
	int i;
	if (! c || (allbut && p))
		i = -1;
	else if (allbut && ! p)
		i = lastto + 1;
	else if (p)
		i = p - from;
	else
		i = -1;
	return (i);
	}
