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
#include "ostreamstr.h"
#include "gcstring.h"
#include "pack.h"
#include <climits>
#include "except.h"
#include <intrin.h> // for _BitScanForward/Reverse

/*
 * Dnum is a decimal floating point implementation
 * using a 64 bit unsigned integer for the coefficient
 * and signed 8 bit integers for the sign and exponent.
 * Max coefficient is 16 decimal digits (not 2^64 - 1 which is 1.8...e19)
 * 16 digits allows splitting into two 8 digit values which fit in 32 bit.
 * Assumed decimal point is to the left of the coefficient.
 * i.e. if exponent is 0, then 0 <= value < 1
 * Zero is represented with a sign of 0. (Zeroed value is zero.)
 * Infinite is represented by a sign of +2 or -2
 * Values are normalized with a maximum coefficient
 * i.e. no leading zero decimal digits
 * since this is the simplest for most operations.
 * From an external viewpoint, Dnum is an immutable value class.
 * Internally, intermediate temporary Dnum values are mutable.
 * 
 * 32 bit programs like cSuneido do not have access to 64 bit instructions.
 * In Visual C++ 64 bit operations are compiled to subroutine calls.
 * So we use 32 bit operations as much as possible.
 * Matching % and / are kept close together
 * since with /O2 VC++ will use a single call to get quotient and remainder.
 */

#define CHECKING false
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

// sign values, zero = 0
enum { NEG_INF = -2, NEG = -1, POS = +1, POS_INF = +2 };

#define COEF_MAX	9'999'999'999'999'999ull
#define MAX_DIGITS	16
#define MAX_SHIFT	(MAX_DIGITS - 1)

#define E1 10
#define E2 100
#define E3 1000
#define E4 10'000
#define E5 100'000
#define E6 1'000'000
#define E7 10'000'000
#define E8 100'000'000

const Dnum Dnum::ZERO;
const Dnum Dnum::ONE(1);
const Dnum Dnum::MINUS_ONE(-1);
// infinite coefficient isn't used, just has to be non-zero for constructor
const Dnum Dnum::INF{ POS_INF, 1, 0 };
const Dnum Dnum::MINUS_INF{ NEG_INF, 1, 0 };
const Dnum Dnum::MAX_INT{ POS, COEF_MAX, 16 };

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

	int clz64(uint64_t n)
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

	// returns 0 to 20
	int ilog10(uint64_t x)
		{
		// based on Hacker's Delight, single 64 bit compare
		if (x == 0)
			return 0;
		int y = (19 * (63 - clz64(x))) >> 6;
		if (y < 19 && x >= pow10[y + 1])
			++y;
		return y;
		}

	// the maximum we can safely shift left (*10)
	int maxShift(uint64_t x)
		{
		int i = ilog10(x);
		return i > MAX_SHIFT ? 0 : MAX_SHIFT - i;
		}

	// "shift" coef "left" as far as possible, returns amount shifted
	int maxCoef(uint64_t& coef)
		{
		int i = maxShift(coef);
		coef *= pow10[i];
		return i;
		}
	}

#define SIGN(n) ((n) < 0 ? -1 : (n) > 0 ? +1 : 0)

#define SETINF() do { \
	sign = s < 0 ? NEG_INF : POS_INF; \
	coef = UINT64_MAX; \
	exp = 0; \
	} while (false)

// used for results of operations, normalizes to max coefficient
Dnum::Dnum(int s, uint64_t c, int e)
	{
	if (s == 0 || c == 0 || e < INT8_MIN)
		*this = Dnum::ZERO;
	else if (s == POS_INF || s == NEG_INF)
		SETINF();
	else
		{
		bool atmax = false;
		while (c > COEF_MAX)
			{
			c = (c + 5) / 10; // drop/round least significant digit
			++e;
			atmax = true;
			}
		if (! atmax)
			e -= maxCoef(c);
		if (e > INT8_MAX)
			SETINF();
		else
			{
			sign = SIGN(s);
			coef = c;
			exp = e;
			}
		}
	}

Dnum::Dnum(int n) 
	: coef(n < 0 ? -n : n), sign(n < 0 ? NEG : n > 0 ? POS : 0)
	{
	if (coef == 0)
		exp = 0;
	else
		exp = MAX_DIGITS - maxCoef(coef);
	}

namespace 
	{
	Dnum inf(int sign)
		{
		return sign > 0 ? Dnum::INF
			: sign < 0 ? Dnum::MINUS_INF
			: Dnum::ZERO;
		}

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

	// returns coef (maximized), advances s, sets exp
	uint64_t get_coef(const char*& s, int& exp)
		{
		bool digits = false;
		bool before_decimal = true;

		// skip leading zeroes, no effect on result
		for (; *s == '0'; ++s)
			digits = true;

		uint64_t n = 0;
		for (int i = MAX_SHIFT; ; ++s)
			{
			if (isdigit(*s))
				{
				digits = true;
				if (*s != '0')
					{
					if (i < 0)
						except("too many digits");
					n += (*s - '0') * pow10[i];
					}
				--i;
				}
			else if (before_decimal)
				{
				// decimal point or end
				exp = MAX_SHIFT - i;
				if (*s != '.')
					break;
				before_decimal = false;
				if (!digits)
					for (; s[1] == '0'; ++s, --exp)
						digits = true;
				}
			else
				break;
			}
		if (!digits)
			except("numbers require at least one digit");
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
// result is left aligned with maximum coef
// returns INF if exponent too large, ZERO if exponent too small
Dnum::Dnum(const char* s)
	{
	sign = POS;
	if (match(s, '-'))
		sign = NEG;
	else
		match(s, '+');
	if (0 == strcmp(s, "inf"))
		{
		*this = sign == NEG ? MINUS_INF : INF;
		return;
		}
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

namespace
	{
	char* append(char* dst, char* src)
		{
		while (*src)
			*dst++ = *src++;
		return dst;
		}
	}

#define PUT(n, p) *d++ = '0' + ((n) / (p)); (n) %= (p)

char* Dnum::tostr(char* buf, int len) const
	{
	verify(len >= STRLEN);
	char* dst = buf;
	if (isZero())
		return strcpy(dst, "0");
	if (sign < 0)
		*dst++ = '-';
	if (isInf())
		return strcpy(dst, "inf"), buf;

	//uint64_t c = coef;
	char digits[17];
	char* d = digits;
	uint32_t hi = uint32_t(coef / E8); // 8 digits
	uint32_t lo = uint32_t(coef % E8); // 8 digits
	PUT(hi, E7);
	PUT(hi, E6);
	PUT(hi, E5);
	PUT(hi, E4);
	PUT(hi, E3);
	PUT(hi, E2);
	PUT(hi, E1);
	PUT(hi, 1);
	PUT(lo, E7);
	PUT(lo, E6);
	PUT(lo, E5);
	PUT(lo, E4);
	PUT(lo, E3);
	PUT(lo, E2);
	PUT(lo, E1);
	PUT(lo, 1);
	while (d > digits && d[-1] == '0')
		--d;
	//for (int i = MAX_SHIFT; i >= 0 && c != 0; --i) 
	//	{
	//	auto p = pow10[i];
	//	int digit = c / p;
	//	c %= p;
	//	*d++ = '0' + digit;
	//	}
	*d = 0;
	int ndigits = d - digits;
	int e = exp - ndigits;
	if (0 <= e && e <= 4)
		{
		// decimal to the right
		dst = append(dst, digits);
		for (int i = 0; i < e; ++i)
			*dst++ = '0';
		}
	else if (-ndigits - 4 < e && e <= -ndigits)
		{
		// decimal to the left
		*dst++ = '.';
		for (int n = -e - ndigits; n > 0; --n)
			*dst++ = '0';
		dst = append(dst, digits);
		}
	else if (-ndigits < e && e <= -1)
		{
		// decimal within
		int dec = ndigits + e;
		for (int i = 0; digits[i]; ++i)
			{
			if (i == dec)
				*dst++ = '.';
			*dst++ = digits[i];
			}
		}
	else
		{
		// scientific notation
		*dst++ = digits[0];
		if (digits[1])
			{
			*dst++ = '.';
			dst = append(dst, digits + 1);
			}
		*dst++ = 'e';
		e += strlen(digits) - 1;
		if (e < 0)
			{
			*dst++ = '-';
			e = -e;
			}
		int nd = e > 100 ? 3 : e > 10 ? 2 : 1;
		switch (nd)
			{
		case 3:
			*dst++ = '0' + (e / 100);
			e %= 100;
			[[fallthrough]];
		case 2:
			*dst++ = '0' + (e / 10);
			e %= 10;
			[[fallthrough]];
		case 1:
			*dst++ = '0' + e;
			break;
		default:
			unreachable();
			}
		}
	*dst = 0;
	return buf;
	}

Ostream& operator<<(Ostream& os, const Dnum& dn)
	{
	char buf[Dnum::STRLEN];
	return os << dn.tostr(buf, sizeof buf);
	}

// "raw" format for test and debug
gcstring Dnum::show() const
	{
	OstreamStr os(32);
	switch (sign)
		{
	case NEG_INF:	os << "--"; break;
	case NEG:		os << "-"; break;
	case 0:			os << "z"; break;
	case POS:		os << "+"; break;
	case POS_INF:	os << "++"; break;
	default:		os << "?"; break;
		}
	if (coef == 0)
		os << '0';
	else
		{
		os << ".";
		uint64_t c = coef;
		for (int i = MAX_SHIFT; i >= 0 && c != 0; --i)
			{
			auto p = pow10[i];
			int digit = int(c / p);
			c %= p;
			CHECK(0 <= digit && digit <= 9);
			os << static_cast<char>('0' + digit);
			}
		}
	os << 'e' << exp;
	return os.gcstr();
	}

gcstring Dnum::to_gcstr() const
	{
	OstreamStr os(32);
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
	// WARNING: assumes/requires both x and y are normalized
	if (x.sign != y.sign)
		return false;
	if (x.sign == 0 || x.sign == NEG_INF || x.sign == POS_INF)
		return true;
	return x.exp == y.exp && x.coef == y.coef;
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
	if (sign >= 0)
		return *this;
	Dnum n(*this);
	n.sign = -n.sign;
	return n;
	}

#define CMP(x, y) ((x) < (y) ? -1 : (x) > (y) ? +1 : 0)

// returns -1 for less than, 0 for equal, +1 for greater than
int Dnum::cmp(const Dnum& x, const Dnum& y)
	{
	if (x.sign < y.sign)
		return -1;
	if (x.sign > y.sign)
		return +1;
	// x.sign == y.sign
	int sign = x.sign;
	if (sign == 0 || sign == NEG_INF || sign == POS_INF)
		return 0;
	if (x.exp < y.exp)
		return -sign;
	if (x.exp > y.exp)
		return +sign;
	return sign * CMP(x.coef, y.coef);
	}

// multiply ---------------------------------------------------------

Dnum operator*(const Dnum& x, const Dnum& y)
	{
	int sign = x.sign * y.sign;
	if (sign == 0)
		return Dnum::ZERO;
	if (x.isInf() || y.isInf())
		return inf(sign);
	int e = x.exp + y.exp;
	if (x.coef == 1)
		return Dnum(sign, y.coef, e);
	if (y.coef == 1)
		return Dnum(sign, x.coef, e);

	uint32_t xhi = uint32_t(x.coef / E8); // 8 digits
	uint32_t xlo = uint32_t(x.coef % E8); // 8 digits

	uint32_t yhi = uint32_t(y.coef / E8); // 8 digits
	uint32_t ylo = uint32_t(y.coef % E8); // 8 digits

	uint64_t c = static_cast<uint64_t>(xhi) * yhi;
	if (xlo == 0)
		{
		if (ylo != 0)
			c += (static_cast<uint64_t>(xhi) * ylo) / E8;
		}
	else if (ylo == 0)
		c += (static_cast<uint64_t>(xlo) * yhi) / E8;
	else
		{
		auto mid1 = (static_cast<uint64_t>(xlo)) * yhi;
		auto mid2 = (static_cast<uint64_t>(ylo)) * xhi;
		c += (mid1 + mid2) / E8;
		}
	return Dnum(sign, c, e);
	}

// divide -----------------------------------------------------------

namespace
	{
	bool mul10safe(uint64_t n)
		{
		return n <= UINT64_MAX / 10;
		}

	// the maximum we can safely shift left (*10)
	int maxShift64(uint64_t x)
		{
		int i = ilog10(x);
		return i > 18 ? 0 : 18 - i;
		}
		
	uint64_t div2(uint64_t x, uint64_t y, int& exp)
		{
		//PRE x maxCoef, x != 0, y mincoef, y != 0
		uint64_t q = 0;
		while (true)
			{
			// ensure x > y so q2 > 0
			if (x < y)
				{
				if (!mul10safe(q))
					break;
				y /= 10; // drop least significant digit
				q *= 10;
				--exp;
				}

			uint64_t q2 = x / y;
			x %= y;
			CHECK(q2 != 0);
			q += q2;
			if (x == 0)
				break;

			// shift x (and q) to the left (max coef)
			// use full 64 bit extra range to reduce iterations
			int p = maxShift64(x > q ? x : q);
			if (p == 0)
				break;
			exp -= p;
			uint64_t pow = pow10[p];
			x *= pow;
			q *= pow;
			}
		return q;
		}

	// count trailing (least significant) zero bits
	int ctz32(uint32_t x)
		{
#if _MSC_VER
		unsigned long idx;
		if (_BitScanForward(&idx, LO32(x)))
			return idx;
		return 32;
#elif __GNUC__
		return n == 0 ? 32 : __builtin_ctz(x);
#else
		int n = 32;
		while (x != 0)
			{
			--n;
			x += x;
			}
		return n; 
#endif
		}

	// "shift" coef right as far as possible, i.e. trim trailing decimal zeroes
	// returns new (larger) exponent
	// minimize slow % operations by using factor of 2 in 10 (ctz)
	// try big steps first to minimize iteration
	int minCoef(uint64_t& coef, int exp)
		{
		// 16 decimal digits = at most 15 trailing decimal zeros
		CHECK(coef != 0);
		int tz = ctz32(LO32(coef));
		if (tz >= 8 && (coef % E8) == 0)
			{
			coef /= E8;
			exp += 8;
			tz -= 8;
			}
		if (tz >= 4 && (coef % E4) == 0)
			{
			coef /= E4;
			exp += 4;
			tz -= 4;
			}
		while (tz > 0 && (coef % 10) == 0)
			{
			coef /= 10;
			exp += 1;
			tz -= 1;
			}
		CHECK(coef % 10 != 0);
		return exp;
		}
	}

Dnum operator/(const Dnum& x, const Dnum& y)
	{
	// special cases
	if (x.isZero())
		return Dnum::ZERO;
	if (y.isZero())
		return inf(x.sign);
	int sign = x.sign * y.sign;
	if (x.isInf())
		{
		if (y.isInf())
			return sign < 0 ? Dnum::MINUS_ONE : Dnum::ONE;
		return inf(sign);
		}
	if (y.isInf())
		return Dnum::ZERO;
	if (y.coef == 1)
		return Dnum(sign, x.coef, x.exp - y.exp);

	uint64_t ycoef = y.coef;
	int yexp = minCoef(ycoef, y.exp);
	int exp = x.exp - yexp + MAX_DIGITS;
	uint64_t q = div2(x.coef, ycoef, exp);
	return Dnum(sign, q, exp);
	}

// add/sub ----------------------------------------------------------

Dnum operator+(const Dnum& x, const Dnum& y)
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

	Dnum yy(y), xx(x);
	if (xx.exp != yy.exp)
		if (!align(xx, yy))
			return xx; // result is larger if exponents too far apart to align
	return xx.sign == yy.sign ? uadd(xx, yy) : usub(xx, yy);
	}

// returns whether or not it was able to align, may swap to ensure x is larger
bool align(Dnum& x, Dnum& y)
	{
	if (x.exp < y.exp)
		std::swap(x, y); // we're adding, so swap doesn't change sign
	CHECK(maxShift(x.coef) == 0);
	int yshift = ilog10(y.coef);
	int e = x.exp - y.exp;
	if (e > yshift)
		return false;
	yshift = e;
	CHECK(0 <= yshift && yshift <= 20);
	y.coef = (y.coef + halfpow10[yshift]) / pow10[yshift];
	CHECK(y.exp + yshift == x.exp);
	return true;
	}

Dnum uadd(const Dnum& x, const Dnum& y)
	{
	// won't overflow 64 bit since we're only using 16 digits
	return Dnum(x.sign, x.coef + y.coef, x.exp);
	}

Dnum usub(const Dnum& x, const Dnum& y)
	{
	return x.coef > y.coef
		? Dnum(x.sign, x.coef - y.coef, x.exp)
		: Dnum(-x.sign, y.coef - x.coef, x.exp);
	}

// packing ----------------------------------------------------------

namespace
	{
	uint64_t prev_coef = 0;
	int prev_size = 0;
	// 8 bytes is sufficient for 16 decimal digits, 2 digits per byte
	// WARNING: static so not threadsafe, must not yield while in use
	uint8_t bytes[8];

	int coef_to_bytes(uint64_t coef)
		{
		// cache the last result, to optimize the common case of packsize,pack
		if (coef == prev_coef)
			return prev_size;
		prev_coef = coef;

		// split 8 digits (32 bits) - 8 digits (32 bits)
		// so each half can be handled as 32 bit
		// and lower precision can be handled in one half
		uint32_t hi = uint32_t(coef / E8); // 8 digits
		uint32_t lo = uint32_t(coef % E8); // 8 digits

		bytes[0] = hi / E6; // most significant
		CHECK(bytes[0]);
		hi %= E6;
		bytes[1] = hi / E4;
		hi %= E4;
		bytes[2] = hi / E2;
		bytes[3] = hi % E2;

		int n = 4;
		if (lo == 0)
			bytes[4] = bytes[5] = bytes[6] = bytes[7] = 0;
		else
			{
			n = 8;
			bytes[4] = lo / E6;
			lo %= E6;
			bytes[5] = lo / E4;
			lo %= E4;
			bytes[6] = lo / E2;
			bytes[7] = lo % E2; // least significant
			}
		while (bytes[n-1] == 0)
			--n;
		return prev_size = n;
		}
	}
	
size_t Dnum::packsize() const
	{
	if (sign == NEG_INF || sign == 0 || sign == POS_INF)
		return 1; // just tag
	return 2 + coef_to_bytes(coef);
	}

void Dnum::pack(char* dst) const
	{
	*dst++ = PACK2_ZERO + sign;
	if (sign == NEG_INF || sign == 0 || sign == POS_INF)
		return;
	uint8_t x = sign < 0 ? 0xff : 0;
	auto e = exp ^ 0x80; // convert to sort as unsigned
	*dst++ = e ^ x;

	int n = coef_to_bytes(coef);
	uint8_t* b = bytes + n;
	dst += n;
	switch (n)
		{
	case 8: *--dst = *--b ^ x; [[fallthrough]];
	case 7: *--dst = *--b ^ x; [[fallthrough]];
	case 6: *--dst = *--b ^ x; [[fallthrough]];
	case 5: *--dst = *--b ^ x; [[fallthrough]];
	case 4: *--dst = *--b ^ x; [[fallthrough]];
	case 3: *--dst = *--b ^ x; [[fallthrough]];
	case 2: *--dst = *--b ^ x; [[fallthrough]];
	case 1: *--dst = *--b ^ x; break;
	default: unreachable();
		}
	}

#define ADD(n_, p_) b = *--src; b ^= x; (n_) += b * (p_)

Dnum Dnum::unpack(const gcstring& s)
	{
	auto src = reinterpret_cast<const uint8_t*>(s.ptr());
	int sign = src[0] - PACK2_ZERO;
	uint8_t x;
	switch (sign)
		{
	case NEG_INF:
		return Dnum::MINUS_INF;
	case NEG:
		x = 0xff;
		break;
	case 0:
		return Dnum::ZERO;
	case POS:
		x = 0;
		break;
	case POS_INF:
		return Dnum::INF;
	default:
		unreachable();
		}
	CHECK(s.size() > 2);
	int8_t exp = static_cast<int8_t>(src[1] ^ 0x80);
	exp ^= x;
	uint32_t hi = 0;
	uint32_t lo = 0;
	uint8_t b;
	src += s.size();
	switch (s.size() - 2)
		{
	case 8: ADD(lo, 1); [[fallthrough]];
	case 7: ADD(lo, E2); [[fallthrough]];
	case 6: ADD(lo, E4); [[fallthrough]];
	case 5: ADD(lo, E6); [[fallthrough]];
	case 4: ADD(hi, 1); [[fallthrough]];
	case 3: ADD(hi, E2); [[fallthrough]];
	case 2: ADD(hi, E4); [[fallthrough]];
	case 1: ADD(hi, E6); break;
	default: unreachable();
		}
	uint64_t coef = static_cast<uint64_t>(hi) * E8 + lo;
	return Dnum(sign, coef, exp);
	}

// tests ------------------------------------------------------------

#include "testing.h"
#include "list.h"
#include <chrono>
#include <utility>
using namespace std::rel_ops;

struct test_dnum : Tests
	{
	TEST(0, "constructors")
		{
		// int
		assert_eq(Dnum::ZERO.show(), "z0e0");
		assert_eq(Dnum::ONE.show(), "+.1e1");
		assert_eq(Dnum(0), Dnum::ZERO);
		assert_eq(Dnum(1234).show(), "+.1234e4");

		// string
		xassert(Dnum("."));
		xassert(Dnum("1.2.3"));
		xassert(Dnum("1111111111111111111111")); // overflow
		xassert(Dnum("-+1"));
		parse("0", "z0e0");
		parse("0.", "z0e0");
		parse(".0", "z0e0");
		parse("0.0", "z0e0");
		parse("-0.0e9", "z0e0");
		parse("9999999999999999", "+.9999999999999999e16");
		parse("1", "+.1e1");
		parse("1234", "+.1234e4");
		parse(".001", "+.1e-2");
		parse("-12.34", "-.1234e2");
		parse("0012.3400", "+.1234e2");
		parse("0012.3400e2", "+.1234e4");
		parse("123000", "+.123e6");
		parse("100.1", "+.1001e3");
		parse("1e18", "+.1e19");
		parse(".9e-9", "+.9e-9");
		parse("-1e-11", "-.1e-10");
		parse("-12.34e56", "-.1234e58");

		assert_eq(Dnum("1e999"), Dnum::INF);
		assert_eq(Dnum("1e-999"), Dnum::ZERO);
		assert_eq(Dnum("0e999"), Dnum::ZERO);
		}
	static void parse(const char* s, const char* expected)
		{
		gcstring ns = Dnum(s).show();
		except_if(ns != expected,
			"parse " << s << " got " << ns << " expected " << expected);
		}
	TEST(1, "to string")
		{
		str(Dnum::ZERO, "0");
		str(Dnum::ONE, "1");
		str(Dnum::INF, "inf");
		str(Dnum::MINUS_INF, "-inf");
		str(Dnum(1234), "1234");
		str(Dnum(-1234), "-1234");
		str("1e9");
		str("1.23e9");
		str("-123.456");
		str(".000123");
		}
	static void str(const Dnum dn, const char* expected)
		{
		char buf[Dnum::STRLEN];
		(void) dn.tostr(buf, sizeof buf);
		except_if(0 != strcmp(buf, expected),
			"tostr\n" << buf << " result\n" << expected << " expected");
		}
	static void str(const char* s)
		{
		Dnum n(s);
		str(n, s);
		}

	TEST(2, "misc")
		{
		assert_eq(clz64(0), 64);
		assert_eq(clz64(0xa), 60);
		assert_eq(clz64(0xff22334455ull), 24);
		assert_eq(clz64(0xff22334455667788ull), 0);

		assert_eq(ilog10(0), 0);
		assert_eq(ilog10(9), 0);
		assert_eq(ilog10(10), 1);
		assert_eq(ilog10(99), 1);
		assert_eq(ilog10(100), 2);
		assert_eq(ilog10(UINT64_MAX), 19);
		uint64_t n = 1;
		for (int i = 0; i < 20; ++i, n *= 10)
			assert_eq(ilog10(n), i);

		assert_eq(maxShift(1), MAX_SHIFT);
		assert_eq(maxShift(9), MAX_SHIFT);
		assert_eq(maxShift(99), MAX_SHIFT - 1);
		assert_eq(maxShift(999'999'999'999'999ull), 1);
		assert_eq(maxShift(1'000'999'999'999'999ull), 0);
		assert_eq(maxShift(9'999'999'999'999'999ull), 0);
		assert_eq(maxShift(UINT64_MAX), 0);

		// normalization
		assert_eq(Dnum(POS, 1, 999), Dnum::INF); // exponent overflow
		assert_eq(Dnum(NEG, 1, 999), Dnum::MINUS_INF); // exponent overflow
		assert_eq(Dnum(POS, 1, -999), Dnum::ZERO); // exponent underflow
		assert_eq(Dnum(NEG, 1, -999), Dnum::ZERO); // exponent underflow
		assert_eq(Dnum("1e127"), Dnum::INF); // exponent overflow

		// unary minus
		assert_eq((-Dnum("0")).show(), "z0e0");
		assert_eq((-Dnum("+123")).to_gcstr(), "-123");
		assert_eq((-Dnum("-123")).to_gcstr(), "123");

		// abs
		assert_eq(Dnum("0").abs().show(), "z0e0");
		assert_eq(Dnum("123").abs().to_gcstr(), "123");
		assert_eq(Dnum("-123").abs().to_gcstr(), "123");
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
		Dnum z("4294967295");
		mul(z, z, "1844674406511962e4");

		mul("112233445566", "112233445566", "1259634630361629e7");

		mul("1111111111111111", "1111111111111111", "1.234567901234568e30");

		mul(Dnum::MAX_INT, Dnum::MAX_INT, "9.999999999999998e31");
		mul(Dnum::MAX_INT, Dnum::ONE, Dnum::MAX_INT);
		mul(Dnum::MAX_INT, "1111111111111111", "1.111111111111111e31");
	
		mul("2e99", "2e99", "inf"); // exp overflow
		}
	template <typename X, typename Y, typename E>
	static void mul(X x, Y y, E expected)
		{
		Dnum dx(x);
		Dnum dy(y);
		Dnum e(expected);
		Dnum p = dx * dy;
		except_if(!almostSame(p, e),
			dx << " * " << dy << "\n" << p << " result\n" << e << " expected");
		p = dy * dx;
		except_if(!almostSame(p, e),
			dy << " * " << dx << "\n" << p << " result\n" << e << " expected");
		}

	TEST(5, "div")
		{
		// ctz
		assert_eq(ctz32(0), 32);
		assert_eq(ctz32(0xfe8), 3);
		assert_eq(ctz32(1), 0);

		// minCoef
		min(123, 123, 0);
		min(123000, 123, 3);
		min(1024, 1024, 0);
		min(1000001, 1000001, 0);
		for (int i = 0; i < 19; ++i)
			min(pow10[i], 1, i);

		// special cases (no actual math)
		div("0", "0", "0");
		div("123", "0", "inf");
		div("123", "inf", "0");
		div("inf", "123", "inf");
		div("inf", "inf", "1");
		div("123456", "1e3", "123.456");

		// divides evenly
		div("123", "123", "1");
		div("4444", "2222", "2");
		div("2222", "4444", ".5");
		div("1", "16", ".0625");
		div("123000", ".000123", "1e9");

		// long division
		div("1", "3", ".3333333333333333");
		div("2", "3", ".6666666666666666");
		div("11", "17", ".6470588235294118");
		div("1234567890123456", "9876543210987654", ".1249999988609374");
		div("1", "3333333333333333", "3e-16");
		div("12", ".4444444444444444", "27");

		// exp overflow
		div("1e99", "1e-99", "inf");
		div("1e-99", "1e99", "0");
		}
	static void min(uint64_t coef, uint64_t out, int exp)
		{
		int e = minCoef(coef, 0);
		assert_eq(e, exp);
		assert_eq(coef, out);
		}
	static void div(const char* x, const char* y, const char* expected)
		{
		Dnum q = Dnum(x) / Dnum(y);
		Dnum e(expected);
		except_if(! almostSame(q, e),
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

		addsub("1e4", "2e2", 10200); // align
		addsub("2e4", "1e2", 20100); // align
		addsub("1e30", 999, "1e30"); // can't align
		addsub("1e15", 3, "1000000000000003");
		addsub("1e16", 33, "10000000000000030"); // dropped digit
		addsub("1e16", 37, "10000000000000040"); // round dropped digit
		addsub("1111111111111111", "2222222222222222e-4", "1111333333333333");
		addsub("1111111111111111", "6666666666666666e-4", "1111777777777778");

		assert_eq(Dnum("7777777777777777") + Dnum("7777777777777777"),
			Dnum("15555555555555550")); // overflow handled by normalization
		assert_eq(Dnum::MAX_INT + Dnum::MAX_INT, Dnum("2e16")); // overflow
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

	TEST(7, "packing")
		{
		const char* nums[] = { "-inf", "-1e9", "-123.45", "-123", "-100",
			"-1e-9", "0", "1e-9", ".123", "100", "123", "123.45", "98765432", 
			"98765432.12345678", "1e9", "inf" };
		List<gcstring> packed;
		for (auto s : nums)
			{
			Dnum dn(s);
			int n = dn.packsize();
			verify(n < 20);
			char buf[20];
			dn.pack(buf);
			gcstring p(buf, n);
			packed.add(p);
			Dnum d2 = Dnum::unpack(p);
			assert_eq(d2, dn);
			}
		for (int i = 0; i < packed.size(); ++i)
			for (int j = 0; j < packed.size(); ++j)
				except_if(CMP(packed[i], packed[j]) != CMP(i, j),
					"packed " << nums[i] << " <=> " << nums[j]);
		}

#define TRUNC(n) ((n) / 10)
#define ROUND(n) (((n) + 5) / 10)

	// for tests, rounds off last digit
	static bool almostSame(const Dnum& x, const Dnum& y)
		{
		return x.sign == y.sign && x.exp == y.exp &&
			(TRUNC(x.coef) == TRUNC(y.coef) || ROUND(x.coef) == ROUND(y.coef));
		}
	};
	REGISTER(test_dnum);

BENCHMARK(dnum_div)
	{
	Dnum x(12345678);
	Dnum y(87654321);
	while (nreps-- > 0)
		(void)(x / y);
	}

BENCHMARK(dnum_div2)
	{
	Dnum x("1234567890123456");
	Dnum y("9876543210987654");
	while (nreps-- > 0)
		(void)(x / y);
	}

BENCHMARK(dnum_mul)
	{
	Dnum x(12345678);
	Dnum y(87654321);
	while (nreps-- > 0)
		(void)(x * y);
	}

BENCHMARK(dnum_mul2)
	{
	Dnum x("1234567890123456");
	Dnum y("9876543210987654");
	while (nreps-- > 0)
		(void)(x * y);
	}

BENCHMARK(dnum_add)
	{
	Dnum x("123456.78");
	Dnum y("876543.21");
	while (nreps-- > 0)
		(void)(x + y);
	}

BENCHMARK(dnum_add2)
	{
	Dnum x("1234567890123456");
	Dnum y("987654321.0987654");
	while (nreps-- > 0)
		(void)(x + y);
	}

BENCHMARK(dnum_pack)
	{
	Dnum x(12345678);
	char buf[20];
	while (nreps-- > 0)
		{
		int n = x.packsize();
		x.pack(buf);
		(void) Dnum::unpack(gcstring::noalloc(buf, n));
		prev_coef = 0; // clear cache
		}
	}

BENCHMARK(dnum_pack2)
	{
	Dnum x("9876543210987654");
	char buf[20];
	while (nreps-- > 0)
		{
		int n = x.packsize();
		x.pack(buf);
		(void) Dnum::unpack(gcstring::noalloc(buf, n));
		prev_coef = 0; // clear cache
		}
	}
