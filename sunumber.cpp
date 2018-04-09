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

#include "sunumber.h"
#include <ctype.h>
#include <algorithm>
using namespace std;
#include "random.h"
#include "except.h"
#include "ostream.h"
#include "pack.h"
#include "sustring.h"
#include "suobject.h"
#include "globals.h"
#include <math.h>
#include <errno.h>
#include "itostr.h"
#include "gc.h"

SuNumber SuNumber::one(1);
SuNumber SuNumber::zero(0L);
SuNumber SuNumber::minus_one(-1);
SuNumber SuNumber::infinity(PLUS, SCHAR_MAX);
SuNumber SuNumber::minus_infinity(MINUS, SCHAR_MAX);

// misc. routines --------------------------------------------

SuNumber::SuNumber(char s, schar e) : sign(s), exp(e)
	{
	memset(digits, 0, sizeof digits);
	check();
	}

SuNumber::SuNumber(char s, schar e, const short* d) : sign(s), exp(e)
	{
	memcpy(digits, d, sizeof digits);
	check();
	}

void* SuNumber::operator new(size_t n)
	{
	return ::operator new (n, noptrs);
	}

void SuNumber::out(Ostream& os) const
	{ os << *this; }

int SuNumber::symnum() const
	{
	int n = integer();
	if (0 <= n && n < 0x8000)
		return n;
	else
		return SuValue::symnum();
	}

bool SuNumber::int_if_num(int* pn) const
	{
	SuNumber num(this);
	num.toint();
	if (num != *this)
		return false;
	*pn = integer();
	return true;
	}

size_t SuNumber::hashfn() const
	{
	// have to ensure that hashfn() == integer() for SHRT_MIN -> SHRT_MAX
	if (is_zero())
		return 0;
	if (exp == 1)
		return sign == PLUS ? digits[0] : -digits[0];
	if (exp == 2)
		{
		int result = digits[0] * 10000L + digits[1];
		return sign == PLUS ? result : -result;
		}
	return digits[0] ^ digits[1] ^ digits[2] ^ digits[3];
	}

#ifdef NDEBUG
inline void SuNumber::check() const
	{ }
#else
// check that a number is valid
void SuNumber::check() const
	{
	if (is_zero() || is_infinity())
		return ;
	verify(sign == PLUS || sign == MINUS);
	verify(SCHAR_MIN < exp && exp < SCHAR_MAX);
	verify(1 <= digits[0] && digits[0] < 10000);
	verify(0 <= digits[1] && digits[1] < 10000);
	verify(0 <= digits[2] && digits[2] < 10000);
	verify(0 <= digits[3] && digits[3] < 10000);
	}
#endif

void SuNumber::shift_right()
	{
	// round
	if (digits[3] >= 5000)
		if (++digits[2] > 9999)
			{
			digits[2] = 0;
			if (++digits[1] > 9999)
				{
				digits[1] = 0;
				++digits[0];
				}
			}
	digits[3] = digits[2];
	digits[2] = digits[1];
	digits[1] = digits[0] % 10000;
	digits[0] /= 10000;
	++exp;
	}

void SuNumber::shift_left()
	{
	verify(digits[0] == 0);
	digits[0] = digits[1];
	digits[1] = digits[2];
	digits[2] = digits[3];
	digits[3] = 0;
	--exp;
	}

// conversions to and from integers

SuNumber::SuNumber(long sx)
	{
	if (sx == 0)
		{
		sign = PLUS;
		exp = SCHAR_MIN;
		digits[0] = digits[1] = digits[2] = digits[3] = 0;
		return ;
		}
	unsigned long x;

	if (sx < 0)
		{ sign = MINUS; x = -sx; }
	else
		{ sign = PLUS; x = sx; }

	if (x < 10000)
		{
		exp = 1;
		digits[0] = x;
		digits[1] = 0;
		digits[2] = 0;
		digits[3] = 0;
		}
	else if (x < 100000000)
		{
		exp = 2;
		digits[0] = x / 10000;
		digits[1] = x % 10000;
		digits[2] = 0;
		digits[3] = 0;
		}
	else
		{
		exp = 3;
		digits[0] = x / 100000000;
		x %= 100000000;
		digits[1] = x / 10000;
		digits[2] = x % 10000;
		digits[3] = 0;
		}
	check();
	}

const uint64 ONE12ZEROS = 1000000000000LL;

SuNumber* SuNumber::from_int64(int64 sx)
	{
	SuNumber* n = new SuNumber(0L);
	if (sx == 0)
		{
		n->sign = PLUS;
		n->exp = SCHAR_MIN;
		n->digits[0] = n->digits[1] = n->digits[2] = n->digits[3] = 0;
		return n;
		}
	uint64 x;

	if (sx < 0)
		{ n->sign = MINUS; x = -sx; }
	else
		{ n->sign = PLUS; x = sx; }

	if (x < 10000)
		{
		n->exp = 1;
		n->digits[0] = x;
		n->digits[1] = 0;
		n->digits[2] = 0;
		n->digits[3] = 0;
		}
	else if (x < 100000000)
		{
		n->exp = 2;
		n->digits[0] = x / 10000;
		n->digits[1] = x % 10000;
		n->digits[2] = 0;
		n->digits[3] = 0;
		}
	else if (x < ONE12ZEROS)
		{
		n->exp = 3;
		n->digits[0] = x / 100000000;
		x %= 100000000;
		n->digits[1] = x / 10000;
		n->digits[2] = x % 10000;
		n->digits[3] = 0;
		}
	else
		{
		n->exp = 4;
		n->digits[0] = x / ONE12ZEROS;
		x %= ONE12ZEROS;
		n->digits[1] = x / 100000000;
		x %= 100000000;
		n->digits[2] = x / 10000;
		n->digits[3] = x % 10000;
		}
	n->check();
	return n;
	}

// WARNING: if exp is 3 but number > LONG_MAX you get undefined results
int SuNumber::integer() const
	{
	if (is_zero() || exp < 1)
		return 0;
	else if (exp > 3)
		return sign == PLUS ? INT_MAX : INT_MIN;			// out of range
	long result = 0;
	if (exp == 1)
		result = digits[0];
	else if (exp == 2)
		result = digits[0] * 10000L + digits[1];
	else if (exp == 3)
		result = digits[0] * 100000000L + digits[1] * 10000L + digits[2];
	return sign == PLUS ? result : -result;
	}

int64 SuNumber::bigint() const
	{
	if (is_zero() || exp < 1)
		return 0;
	else if (exp > 4)
		return sign == PLUS ? LLONG_MAX : LLONG_MIN;	// out of range
	int64 result = 0;
	for (int i = 0; i < exp; ++i)
		{
		result *= 10000;
		result += digits[i];
		}
	return sign == PLUS ? result : -result;
	}

SuNumber* neg(Value x)
	{
	return neg(force<SuNumber*>(x));
	}

// add --------------------------------------------------------------

SuNumber* add(const SuNumber* x, const SuNumber* y)
	{
	if (x->sign == PLUS)
		if (y->sign == PLUS)
			return uadd(x, y);
		else // y.sign == MINUS
			return usub(x, y, new SuNumber(0L));
	else // x.sign == MINUS
		if (y->sign == MINUS)
			return uadd(x, y)->negate();
		else // y.sign == PLUS
			return usub(y, x, new SuNumber(0L));
	}

// unsigned add - result is always plus
SuNumber* uadd(const SuNumber* x, const SuNumber* y)
	{
	if (x->exp > y->exp)
		swap(x, y);
	verify(x->exp <= y->exp);
	if (y->exp - x->exp > NDIGITS)
		return new SuNumber(PLUS, y->exp, y->digits);
	SuNumber* z = new SuNumber(x);
	z->sign = PLUS;
	while (z->exp < y->exp)
		z->shift_right();
	verify(z->exp == y->exp);
	short* zd = z->digits;
	if ((zd[3] += y->digits[3]) > 9999)
		{ zd[3] -= 10000; ++zd[2]; }
	if ((zd[2] += y->digits[2]) > 9999)
		{ zd[2] -= 10000; ++zd[1]; }
	if ((zd[1] += y->digits[1]) > 9999)
		{ zd[1] -= 10000; ++zd[0]; }
	if ((zd[0] += y->digits[0]) > 9999)
		{
		z->shift_right();
		if (z->exp == SCHAR_MAX)
			return &SuNumber::infinity;
		}
	z->check();
	return z;
	}

// subtract ---------------------------------------------------------

SuNumber* sub(const SuNumber* x, const SuNumber* y)
	{
	if (x->sign == PLUS)
		if (y->sign == PLUS)
			return usub(x, y, new SuNumber(0L));
		else // y.sign == MINUS
			return uadd(x, y);
	else // x.sign == MINUS
		if (y->sign == MINUS)
			return usub(x, y, new SuNumber(0L))->negate();
		else // y.sign == PLUS
			return uadd(y, x)->negate();
	}

// unsigned subtract
// treats x and y as positive, returns MINUS if x < y
SuNumber* usub(const SuNumber* x, const SuNumber* y, SuNumber* z)
	{
	// put SMALLER exponent into result so it can be shifted
	char sign = PLUS;
	if (x->exp > y->exp)
		{ swap(x, y); sign = MINUS; }
	verify(x->exp <= y->exp);
	if (y->exp - x->exp > NDIGITS)
		{
		*z = *y;
		z->sign = 1 - sign;
		return z;
		}
	*z = *x;
	z->sign = sign;
	while (z->exp < y->exp)
		z->shift_right();
	verify(z->exp == y->exp);
	short* zd = z->digits;
	if ((zd[3] -= y->digits[3]) < 0)
		{ zd[3] += 10000; --zd[2]; }
	if ((zd[2] -= y->digits[2]) < 0)
		{ zd[2] += 10000; --zd[1]; }
	if ((zd[1] -= y->digits[1]) < 0)
		{ zd[1] += 10000; --zd[0]; }
	if ((zd[0] -= y->digits[0]) < 0)
		{
		zd[0] += 10000;
		// complement z
		short i;
		for (i = 3; i >= 0 && zd[i] == 0; --i)
			;
		verify(i >= 0);
		--zd[i];
		for (; i >= 0; --i)
			zd[i] = 9999 - zd[i];
		z->sign = 1 - z->sign;
		}
	if (zd[0] == 0 && zd[1] == 0 && zd[2] == 0 && zd[3] == 0)
		{ z->sign = PLUS; z->exp = SCHAR_MIN; }
	else
		{
		while (zd[0] == 0)
			{
			z->shift_left();
			verify(z->exp != SCHAR_MIN);
			}
		}
	z->check();
	return z;
	}

// compare ----------------------------------------------------------

static int ord = ::order("Number");

int SuNumber::order() const
	{ return ord; }

bool SuNumber::eq(const SuValue& y) const
	{
	if (auto yy = dynamic_cast<const SuNumber*>(&y))
		return *this == *yy;
	else
		return false;
	}

bool SuNumber::lt(const SuValue& y) const
	{
	int yo = y.order();
	if (yo == ord)
		return *this < static_cast<const SuNumber&>(y);
	else
		return ord < yo;
	}

static_assert(PLUS > MINUS);

int cmp(const SuNumber* x, const SuNumber* y)
	{
	if (x->sign > y->sign)
		return 1;
	else if (x->sign < y->sign)
		return -1;
	else if (x->sign == MINUS)
		return -ucmp(x, y);
	else
		return ucmp(x, y);
	}

int ucmp(const SuNumber* x, const SuNumber* y)
	{
	if (x->exp > y->exp)
		return 1;
	else if (x->exp < y->exp)
		return -1;
	// exponents are equal
	const short* xd = x->digits;
	const short* yd = y->digits;
	short i;
	for (i = 0; i < NDIGITS; ++i)
		if (*xd > *yd)
			return 1;
		else if (*xd++ < *yd++)
			return -1;
	return 0;
	}

bool close(const SuNumber* x, const SuNumber* y)
	{
	if (*x < *y)
		swap(x, y);
	if (x->exp - y->exp > 1)
		return false;
	SuNumber z = *x - *y;
	return x->exp - z.exp >= 3;
	}

// multiply ---------------------------------------------------------

SuNumber* mul(const SuNumber* x, const SuNumber* y)
	{
	if (x->is_zero() || y->is_zero())
		return &SuNumber::zero;
	if (x->is_infinity() || y->is_infinity())
		return x->sign == y->sign ? &SuNumber::infinity : &SuNumber::minus_infinity;
	SuNumber* z = new SuNumber(0L);
	int exp = x->exp + y->exp;
	z->sign = (x->sign == y->sign);

	verify(NDIGITS == 4);
	const short* xd = x->digits;
	long xd3 = xd[3]; long xd2 = xd[2]; long xd1 = xd[1]; long xd0 = xd[0];
	const short* yd = y->digits;
	long yd3 = yd[3]; long yd2 = yd[2]; long yd1 = yd[1]; long yd0 = yd[0];
	const int ZDN = 16;
	long zd[ZDN];
	memset(zd, 0, sizeof zd);
	zd[7] = 				yd3*xd3;
	zd[6] = zd[7] / 10000 + yd3*xd2 + yd2*xd3;
		zd[7] %= 10000;
	zd[5] = zd[6] / 10000 + yd3*xd1 + yd2*xd2 + yd1*xd3;
		zd[6] %= 10000;
	zd[4] = zd[5] / 10000 + yd3*xd0 + yd2*xd1 + yd1*xd2 + yd0*xd3;
		zd[5] %= 10000;
	zd[3] = zd[4] / 10000 +  		  yd2*xd0 + yd1*xd1 + yd0*xd2;
		zd[4] %= 10000;
	zd[2] =	zd[3] / 10000 +  					yd1*xd0 + yd0*xd1;
		zd[3] %= 10000;
	zd[1] =	zd[2] / 10000 +  							  yd0*xd0;
		zd[2] %= 10000;
	zd[0] = zd[1] / 10000;
		zd[1] %= 10000;

	// round zd
	long* zdp = zd;
	while (*zdp == 0)
		++zdp, --exp;
	if (exp <= SCHAR_MIN)
		return &SuNumber::zero;
	if (exp >= SCHAR_MAX)
		return z->sign ? &SuNumber::infinity : &SuNumber::minus_infinity;
	verify(zdp < &zd[ZDN]);
	short i, carry;
round:
	for (carry = zdp[4] > 5000, i = 3; carry && i >= 0; --i)
		{
		zdp[i] += carry;
		if ((carry = (zdp[i] > 9999 ? 1 : 0)) != 0)
			zdp[i] = 0;
		}
	if (carry)
		{
		verify(i == -1);
		verify(zdp > zd);
		--zdp, ++exp;
		verify(*zdp == 0);
		*zdp = 1;
		goto round;
		}
	z->exp = exp;
	for (i = 0; i < NDIGITS; ++i)
		z->digits[i] = zdp[i];
	z->check();
	return z;
	}

// divide -----------------------------------------------------------------

//#include "debug.h"

SuNumber* div(const SuNumber* x, const SuNumber* yy)
	{
	//dbg(*x << " / " << *yy);
	if (x->is_zero())
		return &SuNumber::zero;
	if (x->is_infinity())
		{
		if (yy->is_infinity())
			{
			if (x->sign == yy->sign)
				return &SuNumber::one;
			else
				return &SuNumber::minus_one;
			}
		else
			{
			if (x->sign == yy->sign)
				return &SuNumber::infinity;
			else
				return &SuNumber::minus_infinity;
			}
		}
	if (yy->is_zero())
		return x->sign == PLUS ? &SuNumber::infinity : &SuNumber::minus_infinity;
	if (yy->is_infinity()) // && ! x->is_infinity()
		return &SuNumber::zero;

	SuNumber y(PLUS, 0, yy->digits);
	//dbg("y " << y);
	long y8 = y.first8();
	long y5 = y.first5();
	int guess;

	short zd[MAXDIGITS]; 	// most significant first
	memset(zd, 0, sizeof zd);

	SuNumber remainder(PLUS, 0, x->digits);
	int loops = 0;
	int n = 0;
	SuNumber tmp(0L);
	//dbg("remainder " << remainder);
	while (! remainder.is_zero() && n <= NDIGITS)
		{
		//dbg("-- loop");
		verify(++loops < 3 * NDIGITS);
		if (! (y > remainder))
			{
			//dbg("y <= remainder");
			guess = remainder.first8() / y8;
			guess = max(1, min(guess, 9999 - zd[n]));
			//dbg("guess " << remainder.first8() << " / " << y8 << " = " << guess);
			tmp.product(y, guess);
			//dbg("tmp " << tmp);
			if (tmp > remainder)
				{
				tmp -= y;
				--guess;
				//dbg("adjusted to: guess " << guess << ", tmp " << tmp);
				}
			remainder -= tmp;
			zd[n] += guess;
			verify(zd[n] < 10000);
			}
		else
			{
			//dbg("y > remainder");
			guess = remainder.first9() / y5;
			guess = max(1, min(guess, 9999 - zd[n + 1]));
			//dbg("guess " << guess);
			tmp.product(y, guess);
			--tmp.exp;
			//dbg("tmp " << tmp);
			while (tmp > remainder)
				{
				++tmp.exp;
				tmp -= y;
				verify(tmp.sign == PLUS);
				--tmp.exp;
				--guess;
				//dbg("adjusted to: guess " << guess << ", tmp " << tmp);
				}
			remainder -= tmp;
			verify(remainder.sign == PLUS);
			zd[n + 1] += guess;
			verify(zd[n + 1] < 10000);
			}
		while (remainder.exp < 0 && ! remainder.is_zero())
			{
			++remainder.exp;
			++n;
			}
		//dbg("remainder " << remainder);
		}
	bool sign = (x->sign == yy->sign);
	int exp = x->exp - yy->exp + 1;
	// round zd
	short* zdp = zd;
	while (*zdp == 0)
		++zdp, --exp;
	if (exp <= SCHAR_MIN)
		return &SuNumber::zero;
	if (exp >= SCHAR_MAX)
		return sign ? &SuNumber::infinity : &SuNumber::minus_infinity;
	verify(zdp < &zd[MAXDIGITS]);
	for (short carry = zdp[4] > 5000, i = 3; carry; --i)
		{
		verify(i >= 0);
		zdp[i] += carry;
		if ((carry = (zdp[i] > 9999 ? 1 : 0)) != 0)
			zdp[i] = 0;
		}
	//dbg("");
	return new SuNumber(sign, exp, zdp);
	}

// this = SuNumber * short (only used by divide)
void SuNumber::product(const SuNumber& x, short y)
	{
	exp = x.exp;
	const short* xd = x.digits;
	long tmp;
	tmp = xd[3] * y;
	digits[3] = tmp % 10000;
	tmp = xd[2] * y + tmp / 10000;
	digits[2] = tmp % 10000;
	tmp = xd[1] * y + tmp / 10000;
	digits[1] = tmp % 10000;
	tmp = xd[0] * y + tmp / 10000;
	digits[0] = tmp % 10000;
	if (tmp > 9999)
		{
		shift_right();
		digits[0] += tmp / 10000;
		// += instead of = because shift_right rounding may carry into it
		}
	check();
	}

struct Tmp
	{
	enum { SIZE = 3 * NDIGITS };
	short digits[SIZE];
	Tmp(const short* d)
		{
		memset(digits, 0, sizeof digits);
		for (int i = 0; i < NDIGITS; ++i)
			digits[i + NDIGITS] = d[i];
		}
	short& operator[](int i)
		{
		verify(0 <= i && i <= sizeof (digits));
		return digits[i];
		}
	};

void round2(int digits, int exp, Tmp& tmp, char mode);
void muldiv10(short tmp[], int digits);

SuNumber* round(SuNumber* x, int digits, char mode)
	{
	if (x->is_zero() || digits <= -MAXDIGITS)
		return &SuNumber::zero;
	if (x->is_infinity())
		return &SuNumber::infinity;
	if (digits >= MAXDIGITS)
		return new SuNumber(x); // can't return x because it may be on stack

	Tmp tmp(x->digits);
	round2(digits, x->exp, tmp, mode);

	muldiv10(tmp.digits, -digits);

	// handle round up overflowing
	// e.g. .9.Round(0) => 1 or .995.Round(2) => 1 or 9999.Round(-4) => 10000
	if (tmp[NDIGITS - 1] == 1)
		return new SuNumber(x->sign, x->exp + 1, SuNumber::one.digits);

	// check for zero
	bool zero = true;
	for (int i = NDIGITS; i < 2 * NDIGITS; ++i)
		if (tmp[i] != 0)
			zero = false;
	if (zero)
		return &SuNumber::zero;

	return new SuNumber(x->sign, x->exp, tmp.digits + NDIGITS);
	}

void round2(int digits, int exp, Tmp& tmp, char mode) // also used by mask
	{
	muldiv10(tmp.digits, digits);

	// inc if rounding up
	int ti = NDIGITS + exp;
	if (0 <= ti && ti < Tmp::SIZE && (
		(mode == 'h' && tmp[ti] >= 5000) ||
		(mode == 'u' &&  tmp[ti] > 0)))
		{
		int carry = 1;
		for (int i = NDIGITS + exp - 1; i >= 0; --i)
			{
			int x = carry + tmp[i];
			tmp[i] = x % 10000;
			carry = x / 10000;
			}
		verify(carry == 0);
		}
	// else if mode == 'd'
	// no increment required

	// discard fractional part
	for (int i = 0; i < Tmp::SIZE; ++i)
		if (i >= NDIGITS + exp)
			tmp[i] = 0;
	}

void mul10(short tmp[]);
void div10(short tmp[]);

void muldiv10(short tmp[], int digits)
	{
	if (digits > 0)
		{ // multiply
		for (int d = 0; d < digits; ++d)
			mul10(tmp);
		}
	else if (digits < 0)
		{
		for (int d = 0; d < -digits; ++d)
			div10(tmp);
		}
	// else digits == 0, return unchanged
	}

void mul10(short tmp[])
	{
	int carry = 0;
	for (int i = 3 * NDIGITS - 1; i >= 0; --i)
		{
		verify(0 <= i && i < 3 * NDIGITS);
		int x = tmp[i] * 10 + carry;
		tmp[i] = x % 10000;
		carry = x / 10000;
		}
	verify(carry == 0);
	}

void div10(short tmp[])
	{
	int carry = 0;
	for (int i = 0; i < 3 * NDIGITS; ++i)
		{
		verify(0 <= i && i < 3 * NDIGITS);
		int x = carry * 1000 + tmp[i] / 10;
		carry = tmp[i] % 10;
		tmp[i] = x;
		}
	verify(carry == 0);
	}

// conversions from strings to numbers ------------------------------

// TODO: handle bad format
SuNumber::SuNumber(const char* s)
	{
	exp = 0;

	// sign
	sign = (*s == '-' ? MINUS : PLUS);
	if (*s == '-' || *s == '+')
		++s;

	// skip leading zeroes
	while (*s == '0')
		++s;

	const char* start = s;

	// integer part
	while (isdigit(*s))
		++s;
	short intdigits = s - start;

	// fraction part
	short edif = 0;
	const char* stop = s;
	if (*s == '.')
		{
		++s;
		if (intdigits == 0)
			{
			// skip zeroes after leading decimal
			while (*s == '0')
				++s, --edif;
			start = s;
			}
		while (isdigit(*s))
			++s;
		stop = s;
		}
	else
		{
		// omit trailing zeroes
		while (intdigits > 0 && stop[-1] == '0')
			--stop, ++edif, --intdigits;
		}

	if (intdigits > NDIGITS * 4)
		{
		edif += intdigits - (NDIGITS * 4);
		intdigits = NDIGITS * 4;
		stop = start + intdigits;
		}

	// exponent
	int e = 0;
	if (*s == 'e' || *s == 'E')
		{
		++s;
		bool negexp = false;
		if (*s == '-' || *s == '+')
			negexp = (*s++ == '-');
		while (*s == '0')
			++s;
		while (isdigit(*s))
			e = 10 * e + *s++ - '0';
		if (negexp)
			e = -e;
		}
	e += intdigits + edif;

	// convert the digits
	memset(digits, 0, sizeof digits);
	int i = 1000 + e; // i is not important, just i % 4
	int n = 0;
	for (char* d = (char*) start; d < stop; ++d)
		{
		char c = *d;
		if (c == '.')
			continue ;
		verify(isdigit(c));
 		digits[n] = digits[n] * 10 + (c - '0');
		if (i % 4 == 1)
			if (++n > NDIGITS)
				break ;
		--i;
		}
	if (n < NDIGITS)
		{
		do
			digits[n] *= 10;
			while (i-- % 4 != 1);
		}

	if (e >= 0)
		e = (e + 3) / 4;
	else
		e = e / 4;
	if (e <= SCHAR_MIN)
		{
		*this = zero;
		return ;
		}
	if (e >= SCHAR_MAX)
		{
		*this = sign ? infinity : minus_infinity;
		return ;
		}
	exp = e;

	if (digits[0] == 0 && digits[1] == 0 && digits[2] == 0 && digits[3] == 0)
		{ sign = PLUS; exp = SCHAR_MIN; } // zero
	}

// conversion from SuNumber to string ---------------------------------

Ostream& operator<<(Ostream& out, const SuNumber& num)
	{
	char buf[MAXDIGITS * 4 + 8];

	return out << num.format(buf);
	}

static void put(char*& s, short x)
	{
	for (short j = 0; j < 4; ++j)
		{
		*s++ = '0' + x / 1000;
		x = (x % 1000) * 10;
		}
	}

static void strmove(char* dst, const char* src)
	{
	// not using strcpy because it's undefined for overlapping
	while (0 != (*dst++ = *src++))
		;
	}

static void remove_leading_zeroes(char* num)
	{
	if (num[0] != '0')
		return ;
	char* s = num;
	while (*s == '0')
		++s;
	strmove(num, s);
	}

// TODO: pass buf size
char* SuNumber::format(char* buf) const
	{
	char* s = buf;

	if (is_zero())
		return strcpy(s, "0");
	if (is_infinity())
		return strcpy(s, sign == PLUS ? "Inf" : "-Inf");

	char* start = buf;
	if (sign == MINUS)
		{ *s++ = '-'; ++start; }


	if (-1 <= exp && exp <= NDIGITS + 1)
		{
		short i = 0;
		// print digits before decimal
		for (; i < exp && i < NDIGITS; ++i)
			put(s, digits[i]);
		for (; i < exp; ++i)
			{ *s++ = '0'; *s++ = '0'; *s++ = '0'; *s++ = '0'; }
		if (exp < NDIGITS)
			{
			// print decimal
			*s++ = '.';
			// print possible leading zeros
			for (short j = 0; j < -exp; ++j)
				{ *s++ = '0'; *s++ = '0'; *s++ = '0'; *s++ = '0'; }
			// print digits after decimal
			for (; i < NDIGITS; ++i)
				put(s, digits[i]);
			// skip trailing zeroes
			for (; s - 1 > start && s[-1] == '0'; --s)
				;
			if (s[-1] == '.')
				--s;
			}
		}
	else
		{
		// scientific notation
		*s++ = '.';
		short i;
		for (i = 0; i < NDIGITS; ++i)
			put(s, digits[i]);
		// move decimal to scale 1 <= x < 10
		short x = exp * 4;
		char* t = start;
		do
			{ t[0] = t[1]; t[1] = '.'; --x; }
			while (*t++ == '0');
		// skip trailing zeroes
		for (; s - 1 > start && s[-1] == '0'; --s)
			;
		if (s[-1] == '.')
			--s;
		if (x != 0)
			{
			*s++ = 'e';
			if (x < 0)
				*s++ = '-';
			x = abs(x);
			if (x >= 100)
				*s++ = '0' + (x / 100);
			if (x >= 10)
				*s++ = '0' + ((x % 100) / 10);
			*s++ = '0' + (x % 10);
			}
		}
	*s = 0;
	remove_leading_zeroes(start);
	return buf;
	}

gcstring SuNumber::to_gcstr() const
	{
	char buf[32];
	format(buf);
	return gcstring(buf);
	}

SuNumber& SuNumber::toint()
	{
	if (exp <= 0)
		{
		// result is zero
		sign = PLUS;
		exp = SCHAR_MIN;
		digits[0] = digits[1] = digits[2] = digits[3] = 0;
		}
	else
		{
		for (int i = exp; i < NDIGITS; ++i)
			digits[i] = 0;
		}
	return *this;
	}

SuNumber& SuNumber::tofrac()
	{
	if (exp >= NDIGITS)
		{
		// result is zero
		sign = PLUS;
		exp = SCHAR_MIN;
		digits[0] = digits[1] = digits[2] = digits[3] = 0;
		}
	else
		{
		while (exp > 0)
			{
			digits[0] = 0;
			shift_left();
			}
		if (digits[0] == 0 && digits[1] == 0 && digits[2] == 0 && digits[3] == 0)
			{
			sign = PLUS;
			exp = SCHAR_MIN;
			}
		}
	return *this;
	}

// buf should be as long as mask
char* SuNumber::mask(char* buf, const char* mask) const
	{
	if (is_infinity())
		return strcpy(buf, "#");

	const size_t masksize = strlen(mask);
	char num[4 * NDIGITS * 2 + 1];
	if (is_zero())
		{
		char* dst = num;
		if (auto d = strchr(mask, '.'))
			for (++d; *d == '#'; ++d)
				*dst++ = '0';
		*dst = 0;
		}
	else
		{
		// round to number of decimal places in mask
		if (exp > NDIGITS)
			return strcpy(buf, "#");
		int decimals = 0;
		if (auto d = strchr(mask, '.'))
			for (++d; *d == '#'; ++d)
				++decimals;
		Tmp tmp(digits);
		round2(decimals, exp, tmp, 'h');
		char* dst = num;
		for (int i = 0; i < NDIGITS + exp; ++i)
			put(dst, tmp[i]);
		*dst = 0;
		remove_leading_zeroes(num);
		}
	char* dst = buf + masksize;
	*dst = 0;
	bool signok = (sign == PLUS);
	int i, j;
	for (j = strlen(num) - 1, i = masksize - 1; i >= 0; --i)
		{
		char c = mask[i];
		switch (c)
			{
		case '#' :
			*--dst = (j >= 0 ? num[j--] : '0');
			break ;
		case ',' :
			if (j >= 0)
				*--dst = ',';
			break ;
		case '-' :
		case '(' :
			signok = true;
			if (sign == MINUS)
				*--dst = c;
			break ;
		case ')' :
			*--dst = (sign == MINUS ? c : ' ');
			break ;
		case '.' :
		default :
			*--dst = c;
			break ;
			}
		}
	verify(dst >= buf);

	// strip leading zeros
	char* start = dst;
	while (*start == '-' || *start == '(')
		++start;
	char* end = start;
	while (*end == '0' && end[1])
		++end;
	if (end > start && *end == '.' && (! end[1] || ! isdigit(end[1])))
		--end;
	if (end > start)
		strmove(start, end);

	if (j >= 0)
		return strcpy(buf, "#"); // too many digits for mask
	if (! signok)
		return strcpy(buf, "-"); // negative not handled by mask
	return dst;
	}

SuNumber::SuNumber(random_class)
	{
	sign = ::random(2);
	exp = ::random(21) - 10;
	digits[0] = 1 + ::random(4999); // prevent add_sub from overflow
	digits[1] = ::random(10000);
	digits[2] = ::random(10000);
	digits[3] = ::random(10000);
	check();
	}

// packing ==========================================================

size_t SuNumber::packsize() const
	{
	if (is_zero())
		return 1;
	else if (digits[3])
		return 10;
	else if (digits[2])
		return 8;
	else if (digits[1])
		return 6;
	else if (digits[0])
		return 4;
	else
		return 2; // infinity or minus_infinity
	}

void SuNumber::pack(char* buf) const
	{
	buf[0] = sign == PLUS ? PACK_PLUS : PACK_MINUS;
	if (is_zero())
		return ;
	buf[1] = exp ^ 0x80;
	int digs;
	if (digits[3])
		digs = 4;
	else if (digits[2])
		digs = 3;
	else if (digits[1])
		digs = 2;
	else if (digits[0])
		digs = 1;
	else
		digs = 0;
	if (sign == PLUS)
		{
		if (digs > 0)
			{
			short i = digits[0];
			buf[2] = i >> 8;
			buf[3] = i;
			}
		if (digs > 1)
			{
			short i = digits[1];
			buf[4] = i >> 8;
			buf[5] = i;
			}
		if (digs > 2)
			{
			short i = digits[2];
			buf[6] = i >> 8;
			buf[7] = i;
			}
		if (digs > 3)
			{
			short i = digits[3];
			buf[8] = i >> 8;
			buf[9] = i;
			}
		}
	else // sign == MINUS
		{
		buf[1] = ~buf[1];
		if (digs > 0)
			{
			short i = ~digits[0];
			buf[2] = i >> 8;
			buf[3] = i;
			}
		if (digs > 1)
			{
			short i = ~digits[1];
			buf[4] = i >> 8;
			buf[5] = i;
			}
		if (digs > 2)
			{
			short i = ~digits[2];
			buf[6] = i >> 8;
			buf[7] = i;
			}
		if (digs > 3)
			{
			short i = ~digits[3];
			buf[8] = i >> 8;
			buf[9] = i;
			}
		}
	}

/*static*/ SuNumber* SuNumber::unpack(const gcstring& s)
	{
	int sz = s.size();
	if (sz < 2)
		return &SuNumber::zero;
	verify(sz == 2 || sz == 4 || sz == 6 || sz == 8 || sz == 10);
	SuNumber* n = new SuNumber(0L);
	const uchar* buf = (const uchar*) s.ptr();
	n->sign = buf[0] == PACK_PLUS ? PLUS : MINUS;
	if (n->sign == PLUS)
		{
		n->exp = buf[1] ^ 0x80;
		if (sz >= 4)
			n->digits[0] = (buf[2] << 8) | buf[3];
		if (sz >= 6)
			n->digits[1] = (buf[4] << 8) | buf[5];
		if (sz >= 8)
			n->digits[2] = (buf[6] << 8) | buf[7];
		if (sz == 10)
			n->digits[3] = (buf[8] << 8) | buf[9];
		}
	else // n->sign == MINUS
		{
		n->exp = ~buf[1] ^ 0x80;
		if (sz >= 4)
			n->digits[0] = ~((buf[2] << 8) | buf[3]);
		if (sz >= 6)
			n->digits[1] = ~((buf[4] << 8) | buf[5]);
		if (sz >= 8)
			n->digits[2] = ~((buf[6] << 8) | buf[7]);
		if (sz == 10)
			n->digits[3] = ~((buf[8] << 8) | buf[9]);
		}
	n->check();
	return n;
	}

// literal ==========================================================

/*static*/ Value SuNumber::literal(const char* s)
	{
	if (s[0] == '0' && s[1] == 0)
		return SuZero;
	else if (s[0] == '1' && s[1] == 0)
		return SuOne;
	char* end;
	errno = 0;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) // hex
		{
		unsigned long n = strtoul(s, &end, 16);
		if (*end == 0 && errno != ERANGE)
			return n;
		except("can't convert: " << s);
		}
	long n = strtol(s, &end, 10);
	if (*end == 0 && errno != ERANGE)
		return n;
	return new SuNumber(s);
	}

double SuNumber::to_double() const
	{
	char buf[32];
	format(buf);
	return strtod(buf, NULL);
	}

SuNumber* SuNumber::from_float(float x)
	{
	char buf[32];
	_gcvt(x, 7, buf);
	return new SuNumber(buf);
	}

SuNumber* SuNumber::from_double(double x)
	{
	char buf[32];
	_gcvt(x, 16, buf);
	return new SuNumber(buf);
	}

// call =============================================================

#include "interp.h"

Value SuNumber::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value CHR("Chr");
	static Value INT("Int");
	static Value FRAC("Frac");
	static Value FORMAT("Format");
	static Value SIN("Sin");
	static Value COS("Cos");
	static Value TAN("Tan");
	static Value ASIN("ASin");
	static Value ACOS("ACos");
	static Value ATAN("ATan");
	static Value EXP("Exp");
	static Value LOG("Log");
	static Value LOG10("Log10");
	static Value SQRT("Sqrt");
	static Value POW("Pow");
	static Value HEX("Hex");
	static Value OCTAL("Octal");
	static Value BINARY("Binary");
	static Value ROUND("Round");
	static Value ROUND_UP("RoundUp");
	static Value ROUND_DOWN("RoundDown");

	if (member == FORMAT)
		{
		if (nargs != 1)
			except("usage: number.Format(string)");
		auto format = ARG(0).str();
		char* buf = (char*) _alloca(strlen(format) + 2);
		return new SuString(mask(buf, format));
		}
	else if (member == CHR)
		{
		NOARGS("number.Chr()");
		char buf[2];
		buf[0] = integer();
		buf[1] = 0;
		return new SuString(buf, 1);
		}
	else if (member == INT)
		{
		NOARGS("number.Int()");
		SuNumber* x = new SuNumber(this);
		x->toint();
		return x;
		}
	else if (member == FRAC)
		{
		NOARGS("number.Frac()");
		SuNumber* x = new SuNumber(this);
		x->tofrac();
		return x;
		}
	else if (member == SIN)
		{
		NOARGS("number.Sin()");
		return from_double(sin(to_double()));
		}
	else if (member == COS)
		{
		NOARGS("number.Cos()");
		return from_double(cos(to_double()));
		}
	else if (member == TAN)
		{
		NOARGS("number.Tan()");
		return from_double(tan(to_double()));
		}
	else if (member == ASIN)
		{
		NOARGS("number.ASin()");
		return from_double(asin(to_double()));
		}
	else if (member == ACOS)
		{
		NOARGS("number.ACos()");
		return from_double(acos(to_double()));
		}
	else if (member == ATAN)
		{
		NOARGS("number.ATan()");
		return from_double(atan(to_double()));
		}
	else if (member == EXP)
		{
		NOARGS("number.Exp()");
		return from_double(::exp(to_double()));
		}
	else if (member == LOG)
		{
		NOARGS("number.Log()");
		return from_double(log(to_double()));
		}
	else if (member == LOG10)
		{
		NOARGS("number.Log10()");
		return from_double(log10(to_double()));
		}
	else if (member == SQRT)
		{
		NOARGS("number.Sqrt()");
		return from_double(sqrt(to_double()));
		}
	else if (member == POW)
		{
		if (nargs != 1)
			except("usage: number.Pow(number)");
		return from_double(pow(to_double(), ARG(0).number()->to_double()));
		}
	else if (member == HEX)
		{
		NOARGS("number.Hex()");
		char buf[40];
		utostr(integer(), buf, 16);
		return new SuString(buf);
		}
	else if (member == OCTAL)
		{
		NOARGS("number.Octal()");
		char buf[40];
		utostr(integer(), buf, 8);
		return new SuString(buf);
		}
	else if (member == BINARY)
		{
		NOARGS("number.Binary()");
		char buf[40];
		utostr(integer(), buf, 2);
		return new SuString(buf);
		}
	else if (member == ROUND)
		{
		if (nargs != 1)
			except("usage: number.Round(number)");
		return round(this, ARG(0).integer(), 'h');
		}
	else if (member == ROUND_UP)
		{
		if (nargs != 1)
			except("usage: number.Round(number)");
		return round(this, ARG(0).integer(), 'u');
		}
	else if (member == ROUND_DOWN)
		{
		if (nargs != 1)
			except("usage: number.Round(number)");
		return round(this, ARG(0).integer(), 'd');
		}
	else
		{
		static UserDefinedMethods udm("Numbers");
		if (Value c = udm(member))
			return c.call(self, member, nargs, nargnames, argnames, each);
		else
			method_not_found("number", member);
		}
	}

// test =============================================================

#include "testing.h"

static random_class randnum;

TEST(sunumber_int_convert)
	{
	assert_eq(SuNumber(0L).integer(), 0);
	assert_eq(SuNumber(1).integer(), 1);
	assert_eq(SuNumber(-1).integer(), -1);
	for (long i = 0; i < 1000; ++i)
		{
		long x = rand();
		SuNumber n(x);
		assert_eq(n.integer(), x);
		}
	assert_eq(SuNumber("-.5").integer(), 0);
	assert_eq(SuNumber("1e99").integer(), INT_MAX);
	assert_eq(SuNumber("-1e99").integer(), INT_MIN);
	assert_eq(SuNumber("1e-99").integer(), 0);
	assert_eq(SuNumber("-1e-99").integer(), 0);
	}

TEST(sunumber_str)
	{
	assert_eq(SuNumber("0"), SuNumber(0L));
	assert_eq(SuNumber("1"), SuNumber(1));
	assert_eq(SuNumber("-1"), SuNumber(-1));
	assert_eq(SuNumber("12"), SuNumber(12));
	assert_eq(SuNumber("123"), SuNumber(123));
	assert_eq(SuNumber("1234"), SuNumber(1234));
	assert_eq(SuNumber("12345"), SuNumber(12345));
	assert_eq(SuNumber("1.234e3"), SuNumber(1234));
	assert_eq(SuNumber("0.08"), SuNumber(".08"));
	assert_eq(SuNumber("10000"), SuNumber(10000));
	assert_eq(SuNumber("1000000000000000000"), SuNumber("1e18"));
	assert_eq(SuNumber("1234567890123456789"), SuNumber("1234567890123456000"));
	for (long i = 0; i < 1000; ++i)
		{
		SuNumber x(randnum);
		char buf[30];
		(void) x.format(buf);
		SuNumber y(buf);
		assert_eq(y, x);
		}
	}

TEST(sunumber_add_sub)
	{
	assert_eq(SuNumber(1) + SuNumber(1), SuNumber(2));
	assert_eq(SuNumber(1) - SuNumber(1), SuNumber(0L));
	assert_eq(SuNumber(0L) - SuNumber(1), SuNumber(-1));
	assert_eq(SuNumber(456) - SuNumber(123), SuNumber(333));
	assert_eq(SuNumber(123) - SuNumber(456), SuNumber(-333));
	assert_eq(SuNumber(9999) + SuNumber(1), SuNumber(10000));
	assert_eq(SuNumber(10000) - SuNumber(1), SuNumber(9999));
	assert_eq(SuNumber("999999999999999e489") + SuNumber("999999999999999e489"),
		SuNumber::infinity);
	for (long i = 0; i < 1000; ++i)
		{
		SuNumber x(randnum);
		SuNumber y(randnum);
		SuNumber& a = x + y - y;
		SuNumber& b = x - y + y;
		assert_eq(a, b);
		}
	}

TEST(sunumber_mul)
	{
	assert_eq(SuNumber(0L) * SuNumber(123), SuNumber(0L));
	assert_eq(SuNumber(123) * SuNumber(0L), SuNumber(0L));
	assert_eq(SuNumber(1) * SuNumber(-1), SuNumber(-1));
	assert_eq(SuNumber(-1) * SuNumber(-1), SuNumber(1));
	assert_eq(SuNumber(2) * SuNumber(2), SuNumber(4));
	assert_eq(SuNumber(10001) * SuNumber(10001), SuNumber(100020001));
	assert_eq(SuNumber("1e300") * SuNumber("1e300"), SuNumber::infinity);
	assert_eq(SuNumber("1e-300") * SuNumber("1e-300"), SuNumber::zero);
	assert_eq(SuNumber("1.08") * SuNumber(".9259259259259259"), SuNumber("1"));
	for (long i = 0; i < 1000; ++i)
		{
		{
		long xi = rand() & 0xffff;
		SuNumber x(xi);
		long yi = rand() & 0xffff;
		SuNumber y(yi);
		SuNumber& z = x * y;
		assert_eq(z.integer(), xi * yi);
		}
		SuNumber x(randnum);
		SuNumber y(randnum);
		SuNumber& a = x * y;
		SuNumber& b = y * x;
		assert_eq(a, b);
		}
	}

TEST(sunumber_div)
	{
	assert_eq(SuNumber(123) / SuNumber(0L), SuNumber::infinity);
	assert_eq(SuNumber(0L) / SuNumber(123), SuNumber(0L));
	assert_eq(SuNumber(123) / SuNumber(1), SuNumber(123));
	assert_eq(SuNumber(456) / SuNumber(2), SuNumber(228));
	assert_eq(SuNumber(10000) / SuNumber(10), SuNumber(1000));
	assert_eq(SuNumber("1e-300") / SuNumber("1e300"), SuNumber::zero);
	assert_eq(SuNumber("1e300") / SuNumber("1e-300"), SuNumber::infinity);
	assert_eq(SuNumber(12) / SuNumber(".4444444444444444"), SuNumber(27));
	SuNumber m(100000000), n(100000001);
	verify(! close(&m, &n));
	for (long i = 0; i < 1000; ++i)
		{
		SuNumber x(randnum);
		SuNumber y(randnum);
		SuNumber& a = x / y;
		SuNumber& b = a * y;
		verify(close(&x, &b));
		}
	}
TEST(sunumber_toint)
	{
	assert_eq(SuNumber("0").toint(), SuNumber("0"));
	assert_eq(SuNumber("123").toint(), SuNumber("123"));
	assert_eq(SuNumber("123.456").toint(), SuNumber("123"));
	assert_eq(SuNumber(".456").toint(), SuNumber("0"));
	assert_eq(SuNumber("1e99").toint(), SuNumber("1e99"));
	assert_eq(SuNumber("1e-99").toint(), SuNumber("0"));
	}
TEST(sunumber_tofrac)
	{
	assert_eq(SuNumber("0").tofrac(), SuNumber("0"));
	assert_eq(SuNumber("123").tofrac(), SuNumber("0"));
	assert_eq(SuNumber("123.456").tofrac(), SuNumber(".456"));
	assert_eq(SuNumber(".456").tofrac(), SuNumber(".456"));
	assert_eq(SuNumber("1e99").tofrac(), SuNumber("0"));
	assert_eq(SuNumber("1e-99").tofrac(), SuNumber("1e-99"));
	}

TEST(sunumber_mask)
	{
	char buf[32];
	assert_eq(SuNumber("0").mask(buf, "###"), gcstring("0"));
	assert_eq(SuNumber("0").mask(buf, "#.##"), gcstring(".00"));
	assert_eq(SuNumber(".08").mask(buf, "#.##"), gcstring(".08"));
	assert_eq(SuNumber(".08").mask(buf, "#.#"), gcstring(".1"));
	assert_eq(SuNumber("6.789").mask(buf, "#.##"), gcstring("6.79"));
	assert_eq(SuNumber("123").mask(buf, "##"), gcstring("#"));
	assert_eq(SuNumber("-1").mask(buf, "#.##"), gcstring("-"));
	assert_eq(SuNumber("-12").mask(buf, "-####"), gcstring("-12"));
	assert_eq(SuNumber("-12").mask(buf, "(####)"), gcstring("(12)"));
	}

TEST(sunumber_hash)
	{
	assert_eq(SuNumber(SHRT_MIN).hashfn(), Value(SHRT_MIN).hash());
	assert_eq(SuNumber(-500).hashfn(), Value(-500).hash());
	assert_eq(SuNumber(0L).hashfn(), Value(0).hash());
	assert_eq(SuNumber(500).hashfn(), Value(500).hash());
	assert_eq(SuNumber(SHRT_MAX).hashfn(), Value(SHRT_MAX).hash());
	}

static void ckdbl(double x)
	{
	SuNumber* sunum = SuNumber::from_double(x);
	double y = sunum->to_double();
	verify(fabs(x - y) < 1e-10);
	}
TEST(sunumber_double)
	{
	ckdbl(0);
	ckdbl(1);
	ckdbl(-1);
	ckdbl(123.456);
	ckdbl(-123.456);
	ckdbl(1.234e5);
	ckdbl(1.234e-5);
	ckdbl(-1.234e5);
	ckdbl(-1.234e-5);
	}

TEST(sunumber_int64)
	{
	int64 x = 1;
	for (int i = 0; i < 16; ++i, x = x * 10 + 1)
		{
		SuNumber* n = SuNumber::from_int64(x);
		assert_eq(n->bigint(), x);
		}
	}

static void test(const char* s, const char* expected)
	{
	SuNumber n(s);
	assert_eq(round(&n, 2, 'h')->to_gcstr(), expected);
	}
TEST(sunumber_round)
	{
	test("1e-20", "0");
	test("0", "0");
	test(".11", ".11");
	test(".111", ".11");
	test(".999", "1");
	test("1e20", "1e20");
	}

static void check1(const char* s)
	{
	Value num = SuNumber::literal(s);
	assert_eq(num.to_gcstr(), s);
	}
static void check2(const char* s, const char* t)
	{
	Value num = SuNumber::literal(s);
	assert_eq(num.to_gcstr(), t);
	}
TEST(sunumber_literal)
	{
	check1("0");
	check1("1");
	check1("-1");
	check1("65535"); // USHRT_MAX
	check1("65536");
	check1("3000000000");
	check1("4294967295"); // ULONG_MAX
	check1("5000000000");

	check2("0xffff", "65535");
	check2("0x10000", "65536");
	check2("0xffffffff", "-1");

	check2("0.123", ".123");
	check2("00.001", ".001");
	}

TEST(sunumber_parse)
	{
	assert_eq(SuNumber("10000"), SuNumber(10000));
	assert_eq(SuNumber("1000000000000000000"), SuNumber("1e18"));
	assert_eq(SuNumber("1234567890123456789"), SuNumber("1234567890123456000"));
	}

TEST(sunumber_packed_compare)
	{
	char buf1[100], buf2[100], buf3[100];
	memset(buf1, 0, 100);
	memset(buf2, 0, 100);
	memset(buf3, 0, 100);
	SuNumber n1("-100000"); n1.pack(buf1);
	SuNumber n2("-50000"); n2.pack(buf2);
	SuNumber n3("-1000"); n3.pack(buf3);
	gcstring s1(buf1, n1.packsize());
	gcstring s2(buf2, n2.packsize());
	gcstring s3(buf3, n3.packsize());
	verify(s1 < s2);
	verify(s2 < s3);
	}

BENCHMARK(sunum_div)
	{
	SuNumber x("123456789012.3456");
	SuNumber y("9876543210987654");
	while (nreps-- > 0)
		(void) (x / y);
	}
