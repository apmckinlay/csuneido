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

/*
 * Calculates (e16 * 64 bit) / 64 bit => 64 bit
 * Used by dnum divide
 * Based on jSuneido code
 * which is based on Java BigDecimal code
 * which is based on Hacker's Delight and Knuth TAoCP Vol 2
 * A bit simpler with unsigned types.
 */

#if NDEBUG
#define CHECK(cond)
#else
#include "except.h"
#define CHECK(cond) verify(cond)
#endif

static const uint64_t E16 = 10'000'000'000'000'000ull;
static const uint64_t LONG_MASK = 0xffffffffull;
static const uint64_t DIV_NUM_BASE = (1ull << 32);
static const uint64_t e16_hi = E16 >> 32;
static const uint64_t e16_lo = E16 & LONG_MASK;

int clz64(uint64_t n);

static uint64_t make64(uint32_t hi, uint32_t lo)
	{
	return uint64_t(hi) << 32 | lo;
	}

/** @return u1,u0 - v1,v0 * q0 */
static uint64_t mulsub(uint32_t u1, uint32_t u0, uint32_t v1, uint32_t v0, uint64_t q0)
	{
	uint64_t tmp = u0 - q0 * v0;
	return make64(u1 + (tmp >> 32) - q0 * v1, tmp & LONG_MASK);
	}

static uint64_t divide128(uint64_t dividendHi, uint64_t dividendLo, uint64_t divisor)
	{
	// so we can shift dividend as much as divisor
	// don't allow equals to avoid quotient overflow (by 1)
	CHECK(dividendHi < divisor);

	// maximize divisor (bit wise), since we're mostly using the top half
	int shift = clz64(divisor);
	divisor <<= shift;

	// split divisor
	uint32_t v1 = divisor >> 32;
	uint32_t v0 = divisor & LONG_MASK;

	// matching shift
	uint64_t dls = dividendLo << shift;
	// split dividendLo
	uint32_t u1 = dls >> 32;
	uint32_t u0 = dls & LONG_MASK;

	// tmp1 = top 64 of dividend << shift
	uint64_t tmp1 = (dividendHi << shift) | (dividendLo >> (64 - shift));
	uint64_t q1, r_tmp1;
	if (v1 == 1)
		{
		q1 = tmp1;
		r_tmp1 = 0;
		}
	else
		{
		CHECK(tmp1 >= 0);
		q1 = tmp1 / v1; // DIVIDE top 64 / top 32
		r_tmp1 = tmp1 % v1; // remainder
		}

	// adjust if quotient estimate too large
	CHECK(q1 < DIV_NUM_BASE);
	while (q1*v0 > make64(r_tmp1, u1))
		{
		// done about 5.5 per 10,000 divides
		q1--;
		r_tmp1 += v1;
		if (r_tmp1 >= DIV_NUM_BASE)
			break;
		}
	CHECK(q1 >= 0);
	uint64_t u2 = tmp1 & LONG_MASK; // low half

	// u2,u1 is the MIDDLE 64 bits of the dividend
	uint64_t tmp2 = mulsub(u2, u1, v1, v0, q1);
	uint64_t q0, r_tmp2;
	if (v1 == 1)
		{
		q0 = tmp2;
		r_tmp2 = 0;
		}
	else
		{
		q0 = tmp2 / v1; // DIVIDE dividend remainder 64 / divisor high 32
		r_tmp2 = tmp2 % v1;
		}

	// adjust if quotient estimate too large
	CHECK(q0 < DIV_NUM_BASE);
	while (q0*v0 > make64(r_tmp2, u0))
		{
		// done about .33 times per divide
		q0--;
		r_tmp2 += v1;
		if (r_tmp2 >= DIV_NUM_BASE)
			break;
		}

	return make64(q1, q0);
	}

/// returns (1e16 * dividend) / divisor
uint64_t div128(uint64_t dividend, uint64_t divisor)
	{
	CHECK(dividend != 0);
	CHECK(divisor != 0);
	// multiply dividend * E16
	uint32_t d1_hi = dividend >> 32;
	uint32_t d1_lo = dividend & LONG_MASK;
	uint64_t product = e16_lo * d1_lo;
	uint32_t d0 = product & LONG_MASK;
	uint32_t d1 = product >> 32;
	product = e16_hi * d1_lo + d1;
	d1 = product & LONG_MASK;
	uint64_t d2 = product >> 32;
	product = e16_lo * d1_hi + d1;
	d1 = product & LONG_MASK;
	d2 += product >> 32;
	uint64_t d3 = d2 >> 32;
	d2 &= LONG_MASK;
	product = e16_hi * d1_hi + d2;
	d2 = product & LONG_MASK;
	d3 = ((product >> 32) + d3) & LONG_MASK;
	uint64_t dividendHi = make64(d3, d2);
	uint64_t dividendLo = make64(d1, d0);
	// divide
	return divide128(dividendHi, dividendLo, divisor);
	}

// tests ------------------------------------------------------------

#include "testing.h"

TEST(div128)
	{
	assert_eq(div128(1, 4), 2500000000000000ull);
	assert_eq(div128(1, 3), 3333333333333333ull);
	assert_eq(div128(2, 3), 6666666666666666ull);
	assert_eq(div128(1, 11), 909090909090909ull);
	assert_eq(div128(11, 13), 8461538461538461ull);
	assert_eq(div128(1234567890123456ull, 9876543210987654ull),
		1249999988609374ull);
	}
