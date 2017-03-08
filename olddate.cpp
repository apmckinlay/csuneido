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

#include "sudate.h"
#include "date.h"
#include "gcstring.h"
#include "win.h"
#include "except.h"
#include "pack.h"

static DateTime unpack_old_datetime(const unsigned char* p)
	{
	FILETIME ft;
	ft.dwHighDateTime = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	ft.dwLowDateTime = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];

	SYSTEMTIME st;
	verify(FileTimeToSystemTime(&ft, &st));

	return DateTime(
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond,
		st.wMilliseconds);
	}

SuDate* unpack_old_date(const unsigned char* p)
	{
	DateTime dt = unpack_old_datetime(p);
	return new SuDate(dt.date(), dt.time());
	}

bool is_old_date(const gcstring& s)
	{
	if (s.size() != 9)
		return false;
	const unsigned char* p = (const unsigned char*) s.buf();
	if (*p != PACK_DATE)
		return false;
	++p;

	if (p[0] == 0 && p[1] < 32)
		return false; // new format already
	return true;
	}

void old_to_new_date(gcstring& s)
	{
	if (! is_old_date(s))
		return ;
	const unsigned char* p = (unsigned char*) s.buf() + 1;
	DateTime dt = unpack_old_datetime(p);
	SuDate sudt(dt.date(), dt.time());
	sudt.pack(s.buf());
	}
