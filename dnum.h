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
	Dnum() = default;
	explicit Dnum(int n);
	explicit Dnum(const char* s);
	//Dnum(bool neg, uint64_t c, int8_t e)
	//	: sign(neg ? Sign::NEG : Sign::POS), coef(c), exp(e)
	//	{}

	friend Ostream& operator<<(Ostream& os, const Dnum& dn);
	gcstring show() const;
	gcstring to_gcstr() const;
	bool isZero() const
		{ return sign == Sign::ZERO; }
	bool isInf() const;
	
	static const Dnum ZERO;
	static const Dnum ONE;
	static const Dnum INF;
	static const Dnum MINUS_INF;
private:
	enum class Sign : int8_t { NEG = -1, ZERO = 0, POS = +1 };
	Dnum(Sign s, uint64_t c, int8_t e) : coef(c), sign(s), exp(e)
		{}

	uint64_t coef = 0;
	Sign sign = Sign::ZERO;
	int8_t exp = 0;
	};

