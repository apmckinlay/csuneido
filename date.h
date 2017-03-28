#pragma once
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

struct DateTime
	{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int millisecond;

	DateTime();
	DateTime(int date, int time);
	DateTime(int y, int mo, int d, int h, int mi, int s, int ms);
	bool valid() const;
	int date() const;
	int time() const;
	int day_of_week();
	void add_days(int days)
		{ plus(0, 0, days, 0, 0, 0, 0); }
	void plus(int y, int mo, int d, int h, int mi, int s, int ms);
	int minus_days(DateTime& dt2);
	long long minus_milliseconds(DateTime& dt2);
	};

bool operator==(const DateTime&, const DateTime&);
