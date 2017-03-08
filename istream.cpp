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

#include "istream.h"

Istream::Istream() : eofbit(false), failbit(false), next(-1)
	{ }

Istream& Istream::getline(char* s, int n)
	{
	if (! s || ! n)
		return *this;
	bool read = false; //
	for (;;)
		{
		if (--n <= 0)
			{
			failbit = true;
			break ;
			}
		int c = get();
		if (c == EOFVAL)
			break ;
		read = true;
		if (c == '\n')
			break ;
		*s++ = c;
		}
	if (! read)
		failbit = true;
	*s = 0;
	return *this;
	}

int Istream::get()
	{
	if (next != -1)
		{
		int c = next;
		next = -1;
		return c;
		}
	int c = get_();
	if (c == EOFVAL)
		eofbit = true;
	return c;
	}

Istream& Istream::read(char* buf, int n)
	{
	gcnt = 0;
	if (next != -1)
		{
		*buf++ = next;
		next = -1;
		gcnt = 1;
		}
	gcnt += read_(buf, n - gcnt);
	if (gcnt < n)
		eofbit = true;
	return *this;
	}
