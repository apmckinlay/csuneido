// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "date.h"
#include "except.h"

DateTime::DateTime(int date, int time)
	{
	year = date >> 9;
	month = (date >> 5) & 0xf;
	day = date & 0x1f;

	hour = time >> 22;
	minute = (time >> 16) & 0x3f;
	second = (time >> 10) & 0x3f;
	millisecond = time & 0x3ff;

	verify(valid());
	}

DateTime::DateTime(int y, int mo, int d, int h, int mi, int s, int ms)
	: year(y), month(mo), day(d), hour(h), minute(mi), second(s), millisecond(ms)
	{ }

bool operator==(const DateTime& dt1, const DateTime& dt2)
	{
	return dt1.year == dt2.year &&
		dt1.month == dt2.month &&
		dt1.day == dt2.day &&
		dt1.hour == dt2.hour &&
		dt1.minute == dt2.minute &&
		dt1.second == dt2.second &&
		dt1.millisecond == dt2.millisecond;
	}

int DateTime::date() const
	{
	// 12 bits for year, 4 bits for month, 5 bits for day
	return (year << 9) | (month << 5) | day;
	}

int DateTime::time() const
	{
	// 5 bits for hour, 6 bits for minute, 6 bits for second,
	// 10 bits for millisecond
	return (hour << 22) | (minute << 16) | (second << 10) | millisecond;
	}

// the following code is based on Date:Calc 5.3 by Steffen Beyer (LGPL)

const int DateCalc_Days_in_Year_[2][14] =
	{
    { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    { 0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
	};

const int DateCalc_Days_in_Month_[2][13] =
	{
    { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};

static int DateCalc_Year_to_Days(int year)
	{
    int days;

    days = year * 365L;
    days += year >>= 2;
    days -= year /= 25;
    days += year >>  2;
    return(days);
	}

static bool DateCalc_leap_year(int year)
	{
    int yy;

    return( ((year & 0x03) == 0) &&
            ( (((yy = (int) (year / 100)) * 100) != year) ||
                ((yy & 0x03) == 0) ) );
	}

static int DateCalc_Date_to_Days(int year, int month, int day)
	{
    bool leap;

    if ((year >= 1) &&
        (month >= 1) && (month <= 12) &&
        (day >= 1) &&
        (day <= DateCalc_Days_in_Month_[leap = DateCalc_leap_year(year)][month]))
            return DateCalc_Year_to_Days(--year) +
                    DateCalc_Days_in_Year_[leap][month] + day;
    return 0;
	}

int DateTime::day_of_week()
	{
    int days = DateCalc_Date_to_Days(year, month, day);
    if (days > 0)
        days %= 7;
    return days;
	}

int DateTime::minus_days(DateTime& dt2)
	{
    return DateCalc_Date_to_Days(year, month, day) -
		DateCalc_Date_to_Days(dt2.year, dt2.month, dt2.day);
	}

static void DateCalc_Normalize_Time(int *Dd, int *Dh, int *Dm, int *Ds)
	{
    int quot = *Ds / 60L;
    *Ds -= quot * 60L;
    *Dm += quot;
    quot = *Dm / 60L;
    *Dm -= quot * 60L;
    *Dh += quot;
    quot = *Dh / 24L;
    *Dh -= quot * 24L;
    *Dd += quot;
	}

static void DateCalc_Normalize_Signs(int *Dd, int *Dh, int *Dm, int *Ds)
	{
    int quot;

    quot = *Ds / 86400;
    *Ds -= quot * 86400;
    *Dd += quot;
    if (*Dd != 0L)
		{
        if (*Dd > 0L)
			{
            if (*Ds < 0L)
				{
                *Ds += 86400;
                (*Dd)--;
				}
			}
        else
			{
            if (*Ds > 0L)
				{
                *Ds -= 86400;
                (*Dd)++;
				}
			}
		}
    *Dh = 0L;
    *Dm = 0L;
    if (*Ds != 0L)
		DateCalc_Normalize_Time(Dd,Dh,Dm,Ds);
	}

static bool DateCalc_check_date(int year, int month, int day)
	{
	return (year >= 1) && (year <= 3000) &&
		(month >= 1) && (month <= 12) &&
		(day >= 1) &&
			(day <= DateCalc_Days_in_Month_[DateCalc_leap_year(year)][month]);
	}

static bool DateCalc_check_time(int hour, int min, int sec)
	{
	return (hour >= 0) && (min >= 0) && (sec >= 0) &&
			(hour < 24) && (min < 60) && (sec < 60);
	}

bool DateTime::valid() const
	{
	if (year == 3000 && (month != 1 || day != 1 ||
		hour != 0 || minute != 0 || second != 0 || millisecond != 0))
		return false;
	return DateCalc_check_date(year, month, day) &&
		DateCalc_check_time(hour, minute, second) &&
		0 <= millisecond && millisecond <= 999;
	}

static bool DateCalc_delta_hms(int *Dd,
                           int  *Dh,    int *Dm,   int *Ds,
                           int   hour1, int  min1, int  sec1,
                           int   hour2, int  min2, int  sec2)
	{
    int HH;
    int MM;
    int SS;

    if (DateCalc_check_time(hour1, min1, sec1) &&
        DateCalc_check_time(hour2, min2, sec2))
		{
        SS = ((((hour2 * 60L) + min2) * 60L) + sec2) -
             ((((hour1 * 60L) + min1) * 60L) + sec1);
        DateCalc_Normalize_Signs(Dd, &HH, &MM, &SS);
        *Dh = HH;
        *Dm = MM;
        *Ds = SS;
        return(true);
		}
    return(false);
	}

int64_t DateTime::minus_milliseconds(DateTime& dt2)
	{
	int dd = 0, dh = 0, dm = 0, ds = 0;
	DateCalc_delta_hms(&dd, &dh, &dm, &ds,
		dt2.hour, dt2.minute, dt2.second, hour, minute, second);
	int dms = millisecond - dt2.millisecond;
	return (((((int64_t) minus_days(dt2) + dd) * 24 + dh) * 60 + dm) * 60 + ds) * 1000 + dms;
	}

static void DateCalc_Normalize_Ranges(int *Dd, int *Dh, int *Dm, int *Ds)
	{
    int quot = *Dh / 24L;
    *Dh -= quot * 24L;
    *Dd += quot;
    quot = *Dm / 60L;
    *Dm -= quot * 60L;
    *Dh += quot;
    DateCalc_Normalize_Time(Dd,Dh,Dm,Ds);
	}

static bool DateCalc_add_delta_days(int *year, int *month, int *day, int Dd)
	{
    int days;
    bool leap;

    if (((days = DateCalc_Date_to_Days(*year,*month,*day)) > 0L) &&
        ((days += Dd) > 0L))
		{
        *year = (int) (days / 365.2425);
        *day  = days - DateCalc_Year_to_Days(*year);
        if (*day < 1)
            *day = days - DateCalc_Year_to_Days(*year-1);
        else (*year)++;
        leap = DateCalc_leap_year(*year);
        if (*day > DateCalc_Days_in_Year_[leap][13])
			{
            *day -= DateCalc_Days_in_Year_[leap][13];
            leap  = DateCalc_leap_year(++(*year));
			}
        for ( *month = 12; *month >= 1; (*month)-- )
			{
            if (*day > DateCalc_Days_in_Year_[leap][*month])
				{
                *day -= DateCalc_Days_in_Year_[leap][*month];
                break;
				}
			}
        return true;
		}
    return false;
	}

static bool DateCalc_add_delta_dhms(
	int *year, int *month, int *day, int *hour, int *min, int *sec,
	int Dd, int Dh, int Dm, int Ds)
	{
    if (DateCalc_check_date(*year,*month,*day) &&
        DateCalc_check_time(*hour,*min,*sec))
		{
        DateCalc_Normalize_Ranges(&Dd,&Dh,&Dm,&Ds);
        Ds += ((((*hour * 60L) + *min) * 60L) + *sec) +
               ((( Dh   * 60L) +  Dm)  * 60L);
        while (Ds < 0L)
			{
            Ds += 86400L;
            Dd--;
			}
        if (Ds > 0L)
			{
            Dh = 0L;
            Dm = 0L;
            DateCalc_Normalize_Time(&Dd,&Dh,&Dm,&Ds);
            *hour = Dh;
            *min  = Dm;
            *sec  = Ds;
			}
        else
			*hour = *min = *sec = 0;
		return( DateCalc_add_delta_days(year,month,day,Dd) );
		}
    return false;
	}

static bool DateCalc_add_year_month(int *year, int *month, int Dy, int Dm)
	{
    int quot;

    if ((*year < 1) || (*month < 1) || (*month > 12))
		return false;
    if (Dm != 0L)
		{
        Dm  += *month - 1;
        quot = Dm / 12L;
        Dm  -= quot * 12L;
        if (Dm < 0L)
			{
            Dm += 12L;
            quot--;
			}
        *month = Dm + 1;
        Dy += quot;
		}
    if (Dy != 0L)
		{
        Dy += *year;
        *year = Dy;
		}
    if (*year < 1)
		return false;
    return true;
	}

static bool DateCalc_add_delta_ymdhms(
	int *year, int *month, int *day, int *hour, int *min, int *sec,
	int D_y, int D_m, int D_d, int Dh, int Dm, int Ds)
	{
	if (! (DateCalc_check_date(*year, *month, *day) &&
		DateCalc_check_time(*hour,*min,*sec)))
		return false;
	if (!  DateCalc_add_year_month(year,month,D_y,D_m))
		return false;
    D_d += *day - 1;
    *day = 1;
    return DateCalc_add_delta_dhms(year,month,day,hour,min,sec,D_d,Dh,Dm,Ds);
	}

void DateTime::plus(int y, int mo, int d, int h, int mi, int s, int ms)
	{
	millisecond += ms;
	while (millisecond < 0)
		{
		--s;
		millisecond += 1000;
		}
	while (millisecond >= 1000)
		{
		++s;
		millisecond -= 1000;
		}
	DateCalc_add_delta_ymdhms(&year, &month, &day, &hour, &minute, &second,
		y, mo, d, h, mi, s);
	}

#include "testing.h"

TEST(date)
	{
	DateTime dt(2003, 11, 27, 16, 37, 33, 123);
	DateTime dt2(dt.date(), dt.time());
	assert_eq(dt2.year, 2003);
	assert_eq(dt2.month, 11);
	assert_eq(dt2.day, 27);
	assert_eq(dt2.hour, 16);
	assert_eq(dt2.minute, 37);
	assert_eq(dt2.second, 33);
	assert_eq(dt2.millisecond, 123);

	assert_eq(dt.day_of_week(), 4);

	DateTime dt3(2003, 12, 03, 0, 0, 0, 0);
	assert_eq(dt3.minus_days(dt), 6);

	DateTime dt4(2003, 12, 03, 0, 0, 12, 345);
	assert_eq(dt4.minus_milliseconds(dt3), 12345);

	dt3.plus(0, 0, 0, 1, 2, 3, 4);
	DateTime dt5(2003, 12, 03, 1, 2, 3, 4);
	verify(dt3 == dt5);

	dt.plus(0, 0, 6, 0, 0, 0, 0);
	verify(dt == DateTime(2003, 12, 03, 16, 37, 33, 123));
	}
