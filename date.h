#pragma once
// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <cstdint>

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
	int64_t minus_milliseconds(DateTime& dt2);
	};

bool operator==(const DateTime&, const DateTime&);
