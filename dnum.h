#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2018 Suneido Software Corp.
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

#include <cstdint>

class Ostream;
class gcstring;

class Dnum
	{
public:
	Dnum() : coef(0), sign(0), exp(0)
		{}
	Dnum(const Dnum&) = default;
	explicit Dnum(int n);
	explicit Dnum(const char* s);

	friend struct test_dnum;
	friend Ostream& operator<<(Ostream& os, const Dnum& dn);
	friend bool operator==(const Dnum& x, const Dnum& y);
	static bool almostSame(const Dnum& x, const Dnum& y);
	gcstring show() const;
	gcstring to_gcstr() const;
	bool isZero() const
		{ return sign == 0; }
	bool isInf() const;

	Dnum operator-() const;
	Dnum abs() const;
	static int cmp(const Dnum& x, const Dnum& y);
	friend bool operator<(const Dnum& x, const Dnum& y)
		{ return cmp(x, y) < 0; }
	friend Dnum operator*(const Dnum& x, const Dnum& y);
	friend Dnum operator/(const Dnum& x, const Dnum& y);
	friend Dnum operator+(const Dnum& x, const Dnum& y);
	friend Dnum operator-(const Dnum& x, const Dnum& y)
		{ return x + -y; }

	static const Dnum ZERO;
	static const Dnum ONE;
	static const Dnum MINUS_ONE;
	static const Dnum INF;
	static const Dnum MINUS_INF;
	static const Dnum MAX_INT;
private:
	Dnum(int s, uint64_t c, int e);
	friend bool align(Dnum& x, Dnum& y);
	friend Dnum uadd(const Dnum& x, const Dnum& y);
	friend Dnum usub(const Dnum& x, const Dnum& y);

	uint64_t coef;
	int8_t sign;
	int8_t exp;
	};

