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

#include "dnum.h"
#include "itostr.h"
#include "ostreamstr.h"
#include "gcstring.h"
#include <climits>
#include "except.h"

// Value is sign * coef * 10^exp, zeroed value is 0.

static_assert(sizeof(Dnum) == 16);

#define INF_COEF UINT64_MAX
#define INF_EXP INT8_MAX

const Dnum Dnum::ZERO{Sign::ZERO, 0, 0};
const Dnum Dnum::ONE{Sign::POS, 1, 0};
const Dnum Dnum::INF{Sign::POS, INF_COEF, INF_EXP };
const Dnum Dnum::MINUS_INF{Sign::NEG, INF_COEF, INF_EXP };

Dnum::Dnum(int n) : 
	coef(n < 0 ? -n : n), sign(n < 0 ? Sign::NEG : n > 0 ? Sign::POS : Sign::ZERO)
	{}

// helpers for parsing strings

namespace 
	{
	bool match(const char*& s, char c)
		{
		if (*s != c)
			return false;
		++s;
		return true;
		}

	bool isdigit(char c)
		{
		return '0' <= c && c <= '9';
		}

	// returns coef, advances src, sets exp, non-copying
	uint64_t get_coef(const char*& src, int8_t& exp)
		{
		// skip leading zeroes
		while (*src == '0')
			++src;
		auto s = src;
		bool integer = true;
		for (; isdigit(*src) || *src == '.'; ++src)
			if (*src == '.')
				integer = false;
		// omit trailing zeroes
		auto limit = src;
		for (; limit > s + 1 && limit[-1] == '0'; --limit)
			if (integer)
				++exp;
		if (s == limit)
			except("numbers require at least one digit");
		uint64_t n = 0;
		for (; s < limit; ++s)
			if (*s == '.')
				exp -= limit - s - 1; // potential narrowing
			else
				n = n * 10 + (*s - '0');
		return n;
		}

	void get_exp(const char*& s, int8_t& exp)
		{
		if (match(s, 'e') || match(s, 'E'))
			{
			int esign = match(s, '-') ? -1 : 1;
			match(s, '+');
			int e = 0;
			for (; isdigit(*s); ++s)
				e = e * 10 + (*s - '0');
			exp += e * esign;
			}
		}
	}

Dnum::Dnum(const char* s)
	{
	if (s[0] == '0' && s[1] == 0)
		return; // zero
	sign = match(s, '-') ? Sign::NEG : Sign::POS;
	match(s, '+');
	coef = get_coef(s, exp);
	get_exp(s, exp);
	if (*s) // didn't consume entire string
		except("invalid number");
	}

Ostream& operator<<(Ostream& os, const Dnum& dn)
	{
	if (dn.isZero())
		return os << '0';
	if (dn.sign == Dnum::Sign::NEG)
		os << '-';
	if (dn.isInf())
		return os << "inf";
	char digits[I64BUF + 4];
	u64tostr(dn.coef, digits, 10);
	int ndigits = strlen(digits);
	auto exp = static_cast<int>(dn.exp);
	if (0 <= exp && exp <= 4)
		{
		// decimal to the right
		os << digits;
		for (int i = 0; i < exp; ++i)
			os << '0';
		return os;
		}
	if (-ndigits - 4 < exp && exp <= -ndigits)
		{
		// decimal to the left
		os << ".";
		for (int n = -exp - ndigits; n > 0; --n)
			os << '0';
		os << digits;
		}
	else if (-ndigits < exp && exp <= -1)
		{
		// decimal within
		int d = ndigits + exp;
		for (int i = 0; digits[i]; ++i)
			{
			if (i == d)
				os << '.';
			os << digits[i];
			}
		}
	else
		{
		// use scientific notation
		os << digits[0] << "." << (digits + 1);
		char buf[20];
		os << 'e' << itostr(exp += strlen(digits) - 1, buf, 10);
		}
	return os;
	}

// for debugging
gcstring Dnum::show() const
	{
	OstreamStr os(32);
	os << ((sign == Sign::NEG) ? '-' : (sign == Sign::POS) ? '+' :
		(sign == Sign::ZERO) ? 'z' : '?') <<
		coef << 'e' << exp;
	return os.gcstr();
	}

gcstring Dnum::to_gcstr() const
	{
	OstreamStr os;
	os << *this;
	return os.gcstr();
	}

bool Dnum::isInf() const
	{
	return exp == INF_EXP;
	}

//-------------------------------------------------------------------

#include "testing.h"

struct test_dnum : Tests
	{
	TEST(0, "parse")
		{
		assert_eq(Dnum("1234").show(), "+1234e0");
		assert_eq(Dnum("-12.34").show(), "-1234e-2");
		assert_eq(Dnum("0012.3400").show(), "+1234e-2");
		assert_eq(Dnum("0012.3400e2").show(), "+1234e0");
		assert_eq(Dnum("0").show(), "z0e0");
		assert_eq(Dnum("123000").show(), "+123e3");
		assert_eq(Dnum("100.1").show(), "+1001e-1");
		}
	TEST(1, "ostream")
		{
		assert_eq(Dnum::ZERO.to_gcstr(), "0");
		assert_eq(Dnum::ONE.to_gcstr(), "1");
		assert_eq(Dnum::INF.to_gcstr(), "inf");
		assert_eq(Dnum::MINUS_INF.to_gcstr(), "-inf");
		assert_eq(Dnum(1234).to_gcstr(), "1234");
		assert_eq(Dnum(-1234).to_gcstr(), "-1234");
		}
	};
REGISTER(test_dnum);
