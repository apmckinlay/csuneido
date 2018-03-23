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
#include <algorithm>

/*
 * Dnum is a decimal floating point implementation
 * using a 64 bit unsigned integer for the coefficient
 * and signed 8 bit integers for the sign and exponent.
 * Max coefficient is 1e20 - 1 = 19 nines (not 2^64 - 1 which is 1.8...e19)
 * Value is sign * coef * 10^exp
 * Zero is represented with a sign of 0. (Zeroed value is zero.)
 * Infinite is represented by a sign of +2 or -2
 * Values are normalized with a minimum coefficient
 * i.e. no trailing zero decimal digits
 * since this is the simplest for conversion to/from text.
 * For some operations we maximize the coefficient so it is "left" aligned.
 * This puts the most significant digits at the high end of the coefficient
 * so that 1e18 <= coef < 1e19
 * This format is best for comparison 
 * (and for pack which is required to be comparable)
 * From an external viewpoint, Dnum is an immutable value class.
 * Internally, intermediate temporary Dnum values are mutable.
 */

#pragma warning(disable: 4201)

#define CHECKING true
#if CHECKING
#define CHECK(cond) verify(cond)
#else
#define CHECK(cond)
#endif

typedef struct
	{
	uint32_t lo32;
	uint32_t hi32;
	} Int64;

#define LO32(n) (*reinterpret_cast<Int64*>(&(n))).lo32
#define HI32(n) (*reinterpret_cast<Int64*>(&(n))).hi32

enum { NEG_INF = -2, NEG = -1, POS = +1, POS_INF = +2 }; // zero = 0

static_assert(sizeof(Dnum) == 16); // not 10 due to alignment/padding

#define COEF_MAX	 9'999'999'999'999'999'999ull

const Dnum Dnum::ZERO;
const Dnum Dnum::ONE(1);
const Dnum Dnum::INF{ POS_INF, UINT64_MAX, 0 };
const Dnum Dnum::MINUS_INF{ NEG_INF, UINT64_MAX, 0 };
const Dnum Dnum::MAX_INT{ POS, COEF_MAX, 0 };

namespace
	{
	uint64_t pow10[20] = {
		1ull,
		10ull,
		100ull,
		1000ull,
		10000ull,
		100000ull,
		1000000ull,
		10000000ull,
		100000000ull,
		1000000000ull,
		10000000000ull,
		100000000000ull,
		1000000000000ull,
		10000000000000ull,
		100000000000000ull,
		1000000000000000ull,
		10000000000000000ull,
		100000000000000000ull,
		1000000000000000000ull,
		10000000000000000000ull 
		};
	uint64_t halfpow10[20] = { // for rounding
		0ull,
		5ull,
		50ull,
		500ull,
		5000ull,
		50000ull,
		500000ull,
		5000000ull,
		50000000ull,
		500000000ull,
		5000000000ull,
		50000000000ull,
		500000000000ull,
		5000000000000ull,
		50000000000000ull,
		500000000000000ull,
		5000000000000000ull,
		50000000000000000ull,
		500000000000000000ull,
		5000000000000000000ull 
		};
	}

#define SIGN(n) ((n) < 0 ? -1 : (n) > 0 ? +1 : 0)

#define SETINF() do { \
	sign = s < 0 ? NEG_INF : POS_INF; \
	coef = UINT64_MAX; \
	exp = 0; \
	} while (false)


// primary constructor, normalizes
Dnum::Dnum(int s, uint64_t c, int e) : coef(c), sign(SIGN(s)), exp(e)
	{
	if (s == 0 || c == 0 || e < INT8_MIN)
		*this = Dnum::ZERO;
	else if (s == POS_INF || s == NEG_INF || e > INT8_MAX)
		SETINF();
	else
		{
		e = minCoef();
		if (e > INT8_MAX)
			SETINF();
		else
			exp = e;
		}
	CHECK(coef == 0 || coef % 10 != 0);
	}

Dnum::Dnum(int n) : Dnum(n < 0 ? NEG : n > 0 ? POS : 0, n < 0 ? -n : n, 0)
	{
	}

Dnum Dnum::inf(int sign)
	{
	return sign > 0 ? Dnum::INF
		: sign < 0 ? Dnum::MINUS_INF
		: Dnum::ZERO;
	}

// helpers for char* constructor
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

	bool mul10safe(uint64_t n)
		{
		return n <= COEF_MAX / 10;
		}

	// returns coef, advances src, sets exp, non-copying
	uint64_t get_coef(const char*& src, int& exp)
		{
		auto s = src;
		bool integer = true;
		bool digits = false;
		for (; isdigit(*src) || *src == '.'; ++src)
			if (*src == '.')
				integer = false;
			else
				digits = true;

		// skip leading zeroes
		while (*s == '0')
			++s;
		// omit trailing zeroes
		auto limit = src;
		for (; limit > s + 1 && limit[-1] == '0'; --limit)
			if (integer)
				++exp;
		if (!digits)
			except("numbers require at least one digit");
		uint64_t n = 0;
		for (; s < limit; ++s)
			if (*s == '.')
				exp -= limit - s - 1;
			else if (mul10safe(n))
				{
				uint64_t n2 = n * 10;
				int d = *s - '0';
				if (COEF_MAX - d >= n2)
					n = n2 + d;
				else
					except("number exceeds max");
				}
			else
				except("too many digits");
		CHECK(n <= COEF_MAX);
		return n;
		}

	int get_exp(const char*& s)
		{
		int e = 0;
		if (match(s, 'e') || match(s, 'E'))
			{
			int esign = match(s, '-') ? -1 : 1;
			match(s, '+');
			for (; isdigit(*s); ++s)
				e = e * 10 + (*s - '0');
			e *= esign;
			}
		return e;
		}
	}

// accepts "inf" and "-inf"
// throws for invalid
// result is right aligned with minimum coef, i.e. coef % 10 != 0
// returns INF if exponent too large, ZERO if exponent too small
Dnum::Dnum(const char* s)
	{
	sign = match(s, '-') ? NEG : POS;
	if (0 == strcmp(s, "inf"))
		{
		*this = sign == NEG ? MINUS_INF : INF;
		return;
		}
	match(s, '+');
	int e = 0;
	coef = get_coef(s, e);
	e += get_exp(s);
	if (*s) // didn't consume entire string
		except("invalid number");
	if (coef == 0 || e < INT8_MIN)
		*this = Dnum::ZERO;
	else if (e > INT8_MAX)
		*this = inf(sign);
	else
		exp = e;
	}

Ostream& operator<<(Ostream& os, const Dnum& dn)
	{
	// assumes min coef / right justified
	if (dn.isZero())
		return os << '0';
	if (dn.sign <= NEG)
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
		}
	else if (-ndigits - 4 < exp && exp <= -ndigits)
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
		// scientific notation
		os << digits[0];
		if (digits[1])
			os << "." << (digits + 1);
		char buf[20];
		os << 'e' << itostr(exp += strlen(digits) - 1, buf, 10);
		}
	return os;
	}

// "raw" format for debugging
gcstring Dnum::show() const
	{
	OstreamStr os(32);
	os << ((sign <= NEG) ? '-' : (sign >= POS) ? '+' :
		(sign == 0) ? 'z' : '?') <<
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
	return sign == POS_INF || sign == NEG_INF;
	}

// operations -------------------------------------------------------

bool operator==(const Dnum& x, const Dnum& y)
	{
	// WARNING: assumes both are normalized
	return x.coef == y.coef && x.sign == y.sign && x.exp == y.exp;
	}

#define round(n) (((n) + 5) / 10)

// for tests, rounds off last digit
bool Dnum::almostSame(Dnum x, Dnum y)
	{
	if (x.sign != y.sign)
		return false;
	int xexp = x.maxCoef();
	int yexp = y.maxCoef();
	return xexp == yexp && round(x.coef) == round(y.coef);
	}

// unary minus
Dnum Dnum::operator-() const
	{
	// don't need normalization
	Dnum n(*this);
	n.sign *= -1;
	return n;
	}

Dnum Dnum::abs() const
	{
	// don't need normalization
	Dnum n(*this);
	if (n.sign < 0)
		n.sign = -n.sign;
	return n;
	}

#define CMP(x, y) ((x) < (y) ? -1 : (x) > (y) ? +1 : 0)

// returns -1 for less than, 0 for equal, +1 for greater than
int Dnum::cmp(Dnum x, Dnum y) // by value so we can modify
	{
	if (x.sign < y.sign)
		return -1;
	if (x.sign > y.sign)
		return +1;
	// x.sign == y.sign
	if (x.sign == 0)
		return 0;
	int sign = x.sign;
	if (x.exp != y.exp)
		{
		int xexp = x.maxCoef();
		int yexp = y.maxCoef();
		if (xexp < yexp)
			return -sign;
		if (xexp > yexp)
			return +sign;
		}
	return sign * CMP(x.coef, y.coef);
	}

// multiply ---------------------------------------------------------

namespace
{
	int clz(uint64_t n)
		{
#if _MSC_VER
		unsigned long idx;
		if (_BitScanReverse(&idx, HI32(n)))
			return 31 - idx;
		if (_BitScanReverse(&idx, LO32(n)))
			return 63 - idx;
		return 64;
#elif __GNUC__
		return n == 0 ? 64 : __builtin_clzll(n);
#else
		int c = 0;
		while (n != 0)
			{
			n >>= 1; // shift right assuming more leading zeros
			++c;
			}
		return 64 - c;
#endif
		}

	const uint32_t E7 = 10'000'000;
	const uint32_t E8 = 100'000'000;
	const uint32_t E9 = 1'000'000'000;
	const uint64_t E10 = 10'000'000'000;
	const uint64_t E18 = 1'000'000'000'000'000'000;
}

Dnum operator*(Dnum x, Dnum y) // args by value so we can modify
	{
	int sign = x.sign * y.sign;
	if (sign == 0)
		return Dnum::ZERO;
	if (x.isInf() || y.isInf())
		return Dnum::inf(sign);
	// assuming minCoef

	if (x.coef == 1 || y.coef == 1 || clz(x.coef) + clz(y.coef) >= 64)
		// common, lower precision, fast case
		return Dnum(sign, x.coef * y.coef, x.exp + y.exp);

	int xexp = x.maxCoef();
	uint64_t xhi = x.coef / E9; // 10 digits
	uint32_t xlo = x.coef % E9; // 9 digits

	int yexp = y.maxCoef();
	uint32_t yhi = y.coef / E10; // 9 digits
	uint64_t ylo = y.coef % E10; // 10 digits

	uint64_t hi = xhi * yhi;
	int e = xexp + yexp + 19;
	uint32_t factor = E9;
	uint32_t factor2 = E8;
	if (hi < E18) // optimization to get more precision
		{
		hi *= 10;
		--e;
		factor = E8;
		factor2 = E7;
		}
	uint64_t c;
	if (xlo == 0)
		c = hi + (xhi / 10) * ylo / factor;
	else if (ylo == 0)
		c = hi + (static_cast<uint64_t>(xlo) * yhi) / factor;
	else
		{
		auto mid1 = static_cast<uint64_t>(xlo) * yhi / 10; // 9 dig * 9 dig e10
		auto mid2 = (xhi * ((ylo + 5) / 10)) / 10; // 10 dig e9 * 10 dig
		c = hi + (mid1 + mid2) / factor2; 
		}
	if (factor == E8)
		c += (xlo * ylo + E18/2) / E18;
	return Dnum(sign, c, e);
	}

// divide -----------------------------------------------------------

namespace
	{
	// returns 0 to 20
	int ilog10(uint64_t x)
		{
		// based on Hacker's Delight, single 64 bit compare
		if (x == 0)
			return 0;
		int y = (19 * (63 - clz(x))) >> 6;
		if (y < 20 && x >= pow10[y + 1])
			++y;
		return y;
		}

	// the maximum we can safely shift (*10) x
	int maxShift(uint64_t x)
		{
		int i = ilog10(x);
		return i > 18 ? 0 : 18 - i;
		}

	int div_long = 0;
	int div_iter = 0;
	int div_reduce = 0;
		
	uint64_t div2(uint64_t x, uint64_t y, int& exp)
		{
		//PRE x maxCoef, x != 0, y mincoef, y != 0
		++div_long;
		uint64_t q = 0;
		for (int i = 0; ; ++i)
			{
			++div_iter;
			// ensure x > y so q2 > 0
			if (x < y)
				{
				if (!mul10safe(q))
					goto done;
				y /= 10; // drop least significant digit
				q *= 10;
				--exp;
				}
			if (i == 2 && q < 10000)
				{
				// slow progress, drop another low digit from y
				// reduces worst case iterations
				++div_reduce;
				y /= 10;
				q *= 10;
				--exp;
				}

			uint64_t q2 = x / y;
			CHECK(q2 != 0);
			q += q2;
			x -= q2 * y;
			if (x == 0)
				break;

			// shift x (and q) to the left (max coef)
			int p = maxShift(x > q ? x : q);
			if (p == 0)
				break;
			exp -= p;
			uint64_t pow = pow10[p];
			x *= pow;
			q *= pow;
			}
	done:
		return q;
		}
	}

Dnum operator/(Dnum x, Dnum y) // args by value so we can modify
	{
	// special cases
	if (x.isZero())
		return Dnum::ZERO;
	if (y.isZero())
		return Dnum::inf(x.sign);
	int sign = x.sign * y.sign;
	if (x.isInf())
		{
		if (y.isInf())
			return Dnum(sign, 1, 0);
		return Dnum::inf(sign);
		}
	if (y.isInf())
		return Dnum::ZERO;

	// assuming y.minCoef
	int xexp = x.maxCoef();
	int exp = xexp - y.exp;
	if (x.coef % y.coef == 0)
		// fast, exact path
		return Dnum(sign, x.coef / y.coef, exp);
	// slow path
	uint64_t q = div2(x.coef, y.coef, exp);
	return Dnum(sign, q, exp);
	}

// add/sub ----------------------------------------------------------

Dnum operator+(Dnum x, Dnum y)
	{
	if (x.isZero())
		return y;
	if (y.isZero())
		return x;
	if (x.sign == POS_INF)
		return y.sign == NEG_INF ? Dnum::ZERO : x;
	if (x.sign == NEG_INF)
		return y.sign == POS_INF ? Dnum::ZERO : x;
	if (y.isInf())
		return y;
	// now x and y are not zero or inf
	int exp = x.exp;
	if (x.exp != y.exp)
		if (!align(x, y, exp))
			return x;
	return x.sign == y.sign ? uadd(x, y, exp) : usub(x, y, exp);
	}

// sets resulting exponent
bool align(Dnum& x, Dnum& y, int& exp)
	{
	if (x.exp < y.exp)
		std::swap(x, y); // we're adding, so swap doesn't change sign
	int xshift = maxShift(x.coef);
	int yshift = ilog10(y.coef);
	int e = x.exp - y.exp;
	if (e > xshift + yshift)
		return false;
	if (xshift > e)
		xshift = e;
	CHECK(0 <= xshift && xshift <= 20);
	x.coef *= pow10[xshift];
	exp = x.exp - xshift;
	if (e > xshift)
		{
		yshift = e - xshift;
		CHECK(0 <= yshift && yshift <= 20);
		y.coef = (y.coef + halfpow10[yshift]) / pow10[yshift];
		CHECK(y.exp + yshift == exp);
		}
	return true;
	}

Dnum uadd(const Dnum& x, const Dnum& y, int exp)
	{
	uint64_t coef = x.coef + y.coef;
	if (coef < x.coef || coef < y.coef)
		{ // overflow (note: most overflow will fit in UINT64_MAX)
		auto xc = (x.coef + 5) / 10;
		auto yc = (y.coef + 5) / 10;
		coef = xc + yc;
		++exp;
		}
	return Dnum(x.sign, coef, exp);
	}

Dnum usub(const Dnum& x, const Dnum& y, int exp)
	{
	return x.coef > y.coef
		? Dnum(x.sign, x.coef - y.coef, exp)
		: Dnum(-x.sign, y.coef - x.coef, exp);
	}

// private helpers --------------------------------------------------

// "shift" coef right as far as possible, i.e. trim trailing decimal zeroes
// returns new (larger) exponent
// minimize slow % operations by using factor of 2 in 10
// try big steps first to minimize iteration
// COULD use ilog10 to skip checks for larger divides
int Dnum::minCoef()
	{
	// 19 decimal digits = at most 18 trailing decimal zeros
	int e = exp;
	verify(coef != 0);
	if (coef > COEF_MAX)
		{
		coef = (coef + 5) / 10; // drop/round least significant digit
		++e;
		}
	if ((LO32(coef) & 0b1111'11111) == 0 && (coef % 10000'00000) == 0)
		{
		coef /= 10000'00000;
		e += 9;
		}
	if ((LO32(coef) & 0b11111) == 0 && (coef % 100000) == 0)
		{
		coef /= 100000;
		e += 5;
		}
	while ((LO32(coef) & 1) == 0 && (coef % 10) == 0)
		{
		coef /= 10;
		e += 1;
		}
	CHECK(coef == 0 || coef % 10 != 0);
	return e;
	}

// "shift" coef "left" as far as possible, returns new (smaller) exp
// NOTE: does not adjust this.exp (because it might underflow)
int Dnum::maxCoef()
	{
	if (isZero() || isInf())
		return exp;
	CHECK(0 < coef && coef <= COEF_MAX);
	int i = maxShift(coef);
	coef *= pow10[i];
	CHECK(coef != 0);
	return exp - i;
	}

// tests ------------------------------------------------------------

#include "testing.h"
#include <utility>
using namespace std::rel_ops;

struct test_dnum : Tests
	{
	TEST(0, "from string")
		{
		assert_eq(Dnum("0").show(), "z0e0");
		assert_eq(Dnum("0.").show(), "z0e0");
		assert_eq(Dnum(".0").show(), "z0e0");
		assert_eq(Dnum("0.0").show(), "z0e0");
		assert_eq(Dnum("-0.0e9").show(), "z0e0");
		assert_eq(Dnum("1234").show(), "+1234e0");
		assert_eq(Dnum("-12.34").show(), "-1234e-2");
		assert_eq(Dnum("0012.3400").show(), "+1234e-2");
		assert_eq(Dnum("0012.3400e2").show(), "+1234e0");
		assert_eq(Dnum("123000").show(), "+123e3");
		assert_eq(Dnum("100.1").show(), "+1001e-1");
		assert_eq(Dnum("1e11").show(), "+1e11");
		assert_eq(Dnum("-1e-11").show(), "-1e-11");
		assert_eq(Dnum("9999999999999999999"),
			Dnum(POS, 9999999999999999999ull, 0)); // max
		xassert(Dnum("."));
		xassert(Dnum("1111111111111111111111")); // overflow
		assert_eq(Dnum("1e999"), Dnum::INF);
		assert_eq(Dnum("1e-999"), Dnum::ZERO);
		assert_eq(Dnum("0e999"), Dnum::ZERO);
		}
	TEST(1, "to string")
		{
		assert_eq(Dnum::ZERO.to_gcstr(), "0");
		assert_eq(Dnum::ONE.to_gcstr(), "1");
		assert_eq(Dnum::INF.to_gcstr(), "inf");
		assert_eq(Dnum::MINUS_INF.to_gcstr(), "-inf");
		assert_eq(Dnum(1234).to_gcstr(), "1234");
		assert_eq(Dnum(-1234).to_gcstr(), "-1234");
		assert_eq(Dnum("1e9").to_gcstr(), "1e9");
		}
	TEST(2, "misc")
		{
		// constructors
		assert_eq(Dnum(0), Dnum::ZERO);
		assert_eq(Dnum("-12.34e56"), Dnum(NEG, 1234, 54)); 
		
		// normalization
		assert_eq(Dnum(POS, 1, 999), Dnum::INF); // exponent overflow
		assert_eq(Dnum(NEG, 1, 999), Dnum::MINUS_INF); // exponent overflow
		assert_eq(Dnum(POS, 1, -999), Dnum::ZERO); // exponent underflow
		assert_eq(Dnum(NEG, 1, -999), Dnum::ZERO); // exponent underflow
		assert_eq(Dnum(POS, 123000, 0).show(), "+123e3"); // min coef
		assert_eq(Dnum("1000e127"), Dnum::INF); // min coef exponent overflow

		// unary minus
		assert_eq((-Dnum("0")).show(), "z0e0");
		assert_eq((-Dnum("+123")).show(), "-123e0");
		assert_eq((-Dnum("-123")).show(), "+123e0");

		// abs
		assert_eq(Dnum("0").abs().show(), "z0e0");
		assert_eq(Dnum("123").abs().show(), "+123e0");
		assert_eq(Dnum("-123").abs().show(), "+123e0");

		// minCoef
		assert_eq(Dnum(POS, 1'000, 0).show(), "+1e3");
		assert_eq(Dnum(POS, 1'000'000, 0).show(), "+1e6");
		assert_eq(Dnum(POS, 1'000'000'000, 0).show(), "+1e9");
		assert_eq(Dnum(POS, 1'000'000'000'000ull, 0).show(), "+1e12");

		// maxCoef
		Dnum n(9);
		int p = n.maxCoef();
		assert_eq(p, -18);
		assert_eq(n.coef, 9000000000000000000ull);
		n = Dnum(987654);
		p = n.maxCoef();
		assert_eq(p, -13);
		assert_eq(n.coef, 9876540000000000000ull);
		}
	TEST(3, "cmp")
		{
		Dnum::cmp(Dnum("123.4"), Dnum("123.45"));
		const char* nums[] = { "-inf", "-1e9", "-123.45", "-123", "-100", "-1e-9", 
			"0", "1e-9", "100", "123", "123.45", "1e9", "inf" };
		int i = 0;
		for (auto x : nums)
			{
			int j = 0;
			for (auto y : nums)
				{
				cmp(x, y, CMP(i, j));
				cmp(y, x, CMP(j, i));
				++j;
				}
			++i;
			}
		cmp(Dnum(POS, 1000, 0), Dnum(POS, 1, 3), 0);
		}
	template <typename X, typename Y>
	static void cmp(X x_, Y y_, int c)
		{
		Dnum x(x_);
		Dnum y(y_);
		except_if(Dnum::cmp(x, y) != (c),
			(x) << " <=> " << (y) << " = " << Dnum::cmp(x, y));
		}


	TEST(4, "mul")
		{
		assert_eq(clz(0), 64);
		assert_eq(clz(0xa), 60);
		assert_eq(clz(0xff22334455ull), 24);
		assert_eq(clz(0xff22334455667788ull), 0);

		assert_eq(ilog10(0), 0);
		assert_eq(ilog10(9), 0);
		assert_eq(ilog10(10), 1);
		assert_eq(ilog10(99), 1);
		assert_eq(ilog10(100), 2);
		assert_eq(ilog10(UINT64_MAX), 19);
		uint64_t n = 1;
		for (int i = 0; i < 20; ++i, n *= 10)
			assert_eq(ilog10(n), i);

		assert_eq(maxShift(1), 18);
		assert_eq(maxShift(9), 18);
		assert_eq(maxShift(99), 17);
		assert_eq(maxShift(999'999'999'999'999'999ull), 1);
		assert_eq(maxShift(1'000'999'999'999'999'999ull), 0);
		assert_eq(maxShift(9'999'999'999'999'999'999ull), 0);
		assert_eq(maxShift(UINT64_MAX), 0);

		// special cases (no actual math)
		mul(0, 0, 0);
		mul(0, 123, 0);
		mul(0, "inf", 0);
		mul("inf", 123, "inf");
		mul("inf", "inf", "inf");

		// fast, single multiply
		const int nums[] = { 0, 1, -1, 100, 1234, 9999, -1234 };
		for (auto x : nums)
			for (auto y : nums)
				mul(x, y, x * y);
		Dnum z(POS, UINT32_MAX, 0);
		mul(z, z, "18446744065119617030");

		mul("112233445566", "112233445566",
			"1259634630361628506e4");

		mul("1111111111111111111", "1111111111111111111",
			"123456790123456790e19");

		mul(Dnum::MAX_INT, Dnum::MAX_INT, "9999999999999999998e19");
		mul(Dnum::MAX_INT, Dnum::ONE, Dnum::MAX_INT);
		mul(Dnum::MAX_INT, "1111111111111111111", "1.111111111111111111e37");
		}
	template <typename X, typename Y, typename E>
	static void mul(X x, Y y, E expected)
		{
		Dnum dx(x), dy(y), e(expected);
		for (int i = 0; i < 2; ++i)
			{
			Dnum p = dx * dy;
			except_if(!Dnum::almostSame(p, e),
				dx << " * " << dy << "\n" << p << " result\n" << e << " expected");
			p = -dx * -dy;
			except_if(!Dnum::almostSame(p, e),
				dx << " * " << dy << "\n" << p << " result\n" << e << " expected");
			p = -dx * dy;
			except_if(!Dnum::almostSame(p, -e),
				dx << " * " << dy << "\n" << p << " result\n" << e << " expected");
			p = dx * -dy;
			except_if(!Dnum::almostSame(p, -e),
				dx << " * " << dy << "\n" << p << " result\n" << e << " expected");
			std::swap(dx, dy);
			}
		}

	TEST(5, "div")
		{
		// special cases (no actual math)
		div("0", "0", "0");
		div("123", "0", "inf");
		div("123", "inf", "0");
		div("inf", "123", "inf");
		div("inf", "inf", "1");
		div("123", "123", "1");

		// divides evenly
		div("4444", "2222", "2");
		div("2222", "4444", ".5");
		div("1", "16", ".0625");
		div("123000", ".000123", "1e9");

		// long division
		div("1", "3", ".3333333333333333333");
		div("2", "3", ".6666666666666666666");
		div("11", "17", ".6470588235294117647");
		div("1234567890123456789", "9876543210987654321", ".1249999988609375000");
		div("1", "3333333333333333333", "3e-19");

		// exp overflow
		div("1e99", "1e-99", "inf");
		div("1e-99", "1e99", "0");
		}
	static void div(const char* x, const char* y, const char* expected)
		{
		Dnum q = Dnum(x) / Dnum(y);
		Dnum e(expected);
		except_if(! Dnum::almostSame(q, e),
			x << " / " << y << "\n" << q << " result\n" << e << " expected");
		}

	TEST(6, "add/sub")
		{
		// special cases
		addsub(123, 0, 123);
		addsub("inf", "-inf", 0);
		addsub("inf", 123, "inf");
		addsub("-inf", 123, "-inf");
		assert_eq(Dnum::INF + Dnum::INF, Dnum::INF);
		assert_eq(Dnum::MINUS_INF + Dnum::MINUS_INF, Dnum::MINUS_INF);
		assert_eq(Dnum::INF - Dnum::INF, Dnum::ZERO);
		assert_eq(Dnum::MINUS_INF - Dnum::MINUS_INF, Dnum::ZERO);

		const int nums[] = { 0, 1, 12, 99, 100, 123, 1000, 1234, 9999 };
		for (auto x : nums)
			for (auto y : nums)
				{
				addsub(x, y, x + y);
				addsub(x, -y, x - y);
				}

		addsub(111, 12, 123);
		addsub(1e4, 2e2, 10200); // align
		addsub(2e4, 1e2, 20100); // align
		addsub("1e30", 999, "1e30"); // can't align
		addsub("1e18", 3, "1000000000000000003");
		addsub("1e19", 33, "10000000000000000030"); // dropped digit
		addsub("1e19", 37, "10000000000000000040"); // round dropped digit

		assert_eq(Dnum("7777777777777777777") + Dnum("7777777777777777777"),
			Dnum("15555555555555555550")); // overflow handled by normalization
		assert_eq(Dnum::MAX_INT + Dnum::MAX_INT, Dnum("2e19")); // overflow
		}
	template <typename X, typename Y, typename Z>
	static void addsub(X x_, Y y_, Z sum_)
		{
		Dnum x(x_);
		Dnum y(y_);
		Dnum sum(sum_);
		Dnum z = x + y; // add
		except_if(z != sum,
			x << " + " << y << "\n" << z << " result\n" << sum << " expected");
		z = y + x; // add reverse
		except_if(z != sum,
			y << " + " << x << "\n" << z << " result\n" << sum << " expected");
		z = -x + -y; // add negatives
		except_if(z != -sum,
			-x << " + " << -y << "\n" << z << " result\n" << -sum << " expected");
		z = sum - y; // subtract
		except_if(z != x,
			sum << " - " << y << "\n" << z << " result\n" << x << " expected");
		}
	};
REGISTER(test_dnum);
