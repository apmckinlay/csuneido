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
#include "value.h"
#include "gc.h"
#include "except.h"
#include "pack.h"
#include "gcstring.h"

SuNumber SuNumber::one(Dnum(1));
SuNumber SuNumber::zero(Dnum(0));
SuNumber SuNumber::minus_one(Dnum(-1));
SuNumber SuNumber::infinity(Dnum::inf(+1));
SuNumber SuNumber::minus_infinity(Dnum::inf(-1));

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

void SuNumber::out(Ostream& os) const
	{
	os << dn;
	}

void * SuNumber::operator new(size_t n) // NOLINT
	{
	return ::operator new (n, noptrs);
	}

static int ord = ::order("Number");

int SuNumber::order() const
	{
	return ord;
	}

bool SuNumber::lt(const SuValue& y) const
	{
	if (auto yy = dynamic_cast<const SuNumber*>(&y))
		return dn < yy->dn;
	else
		return ord < y.order();
	}

bool SuNumber::eq(const SuValue& y) const
	{
	if (auto yy = dynamic_cast<const SuNumber*>(&y))
		return dn == yy->dn;
	else
		return false;
	}

char * SuNumber::format(char* buf, const char* mask) const
	{
	Dnum n = dn;
	int masksize = strlen(mask);
	int before = 0;
	int after = 0;
	bool intpart = true;
	for (int i = 0; i < masksize; ++i)
		switch (mask[i])
			{
		case '.':
			intpart = false;
			break;
		case '#':
			if (intpart)
				++before;
			else
				++after;
			break;
		default: ;
			}
	if (n.getexp() > before)
		return strcpy(buf, "#"); // too big to fit in mask

	n = n.round(after);
	char digits[Dnum::MAX_DIGITS + 1];
	int nd = n.getdigits(digits);

	int e = n.getexp();
	if (nd == 0 && after == 0)
		{
		digits[0] = '0';
		e = nd = 1;
		}
	int di = e - before;
	verify(di <= 0);
	char* dst = buf;
	int sign = n.signum();
	bool signok = (sign >= 0);
	bool frac = false;
	for (int mi = 0; mi < masksize; ++mi)
		{
		char mc = mask[mi];
		switch (mc)
			{
			case '#':
				if (0 <= di && di < nd)
					*dst++ = digits[di];
				else if (frac || di >= 0)
					*dst++ = '0';
				++di;
				break;
			case ',':
				if (di > 0)
					*dst++ = ',';
				break;
			case '-':
			case '(':
				signok = true;
				if (sign < 0)
					*dst++ = mc;
				break;
			case ')':
				*dst++ = sign < 0 ? mc : ' ';
				break;
			case '.':
				frac = true;
			default:
				*dst++ = mc;
				break;
			}
		}
	if (!signok)
		return strcpy(buf, "-"); // negative not handled by mask
	*dst = 0;
	return buf;
	}

size_t SuNumber::hashfn() const
	{
	// have to ensure that hashfn() == integer() for SHRT_MIN -> SHRT_MAX
	int n = dn.intOrMin();
	if (SHRT_MIN <= n && n <= SHRT_MAX)
		return n;
	return dn.hashfn();
	}

int SuNumber::symnum() const
	{
	int n = dn.intOrMin();
	if (0 <= n && n < 0x8000)
		return n;
	else
		return SuValue::symnum(); // throw
	}

int SuNumber::integer() const
	{
	//TODO fix stdlib so this can require integer and throw for out of range
	int64_t n = dn.integer().to_int64();
	return (INT_MIN <= n && n <= INT_MAX) ? n : 0;
	}

bool SuNumber::int_if_num(int * pn) const
	{
	int n = dn.intOrMin();
	if (n == INT_MIN)
		return false;
	*pn = n;
	return true;
	}

//===================================================================
// pack/unpack - OLD format

/** 16 digits - maximum precision that cSuneido handles */
static const int64_t MAX_PREC = 9999999999999999L;
static const int64_t MAX_PREC_DIV_10 = 999999999999999L;

static int packshorts(int64_t n) {
	int i = 0;
	for (; n != 0; ++i)
		n /= 10000;
	verify(i <= 4); // cSuneido limit
	return i;
	}

static int packSizeNum(int64_t n, int e) {
	if (n == 0)
		return 1;
	if (n < 0)
		n = -n;
	// strip trailing zeroes
	while ((n % 10) == 0)
		{
		n /= 10;
		e++;
		}
	// adjust e to a multiple of 4 (to match old format)
	while ((e % 4) != 0 && n < MAX_PREC_DIV_10)
		{
		n *= 10;
		--e;
		}
	while ((e % 4) != 0 || n > MAX_PREC)
		{
		n /= 10;
		++e;
		}
	int ps = packshorts(n);
	e = e / 4 + ps;
	if (e >= SCHAR_MAX)
		return 2;
	return 2 /* tag and exponent */ + 2 * ps;
	}

size_t SuNumber::packsize() const
	{
	if (dn.isInf())
		return 2;
	return packSizeNum(dn.getcoef(), dn.getexp() - Dnum::MAX_DIGITS);
	}

//-------------------------------------------------------------------

static int8_t scale(int e, bool minus) {
	int8_t eb = (e ^ 0x80) & 0xff;
	if (minus)
		eb = (~eb) & 0xff;
	return eb;
	}

static short digit(int64_t n, bool minus) {
	return minus ? ~(n & 0xffff) : n;
	}

static void packLongPart(char* dst, int64_t n, bool minus) {
	short sh[4];
	int i;
	for (i = 0; n != 0; ++i)
		{
		sh[i] = digit(n % 10000, minus);
		n /= 10000;
		}
	while (--i >= 0)
		{
		*dst++ = sh[i] >> 8;
		*dst++ = sh[i];
		}
	}

static void packNum(int64_t n, int e, char* dst) {
	bool minus = n < 0;
	*dst++ = minus ? PACK_MINUS : PACK_PLUS;
	if (n == 0)
		return;
	if (n < 0)
		n = -n;
	// strip trailing zeroes
	while ((n % 10) == 0)
		{
		n /= 10;
		e++;
		}
	// make e multiple of 4, and limit precision
	while ((e % 4) != 0 && n < MAX_PREC_DIV_10)
		{
		n *= 10;
		--e;
		}
	while ((e % 4) != 0 || n > MAX_PREC)
		{
		n /= 10;
		++e;
		}
	e = e / 4 + packshorts(n);
	if (e >= SCHAR_MAX)
		{
		*dst = minus ? 0 : 0xff;
		return;
		}
	*dst++ = scale(e, minus);
	packLongPart(dst, n, minus);
	}

void SuNumber::pack(char* buf) const
	{
	if (dn.isInf())
		packNum(dn.signum(), 4 * SCHAR_MAX, buf);
	else
		packNum(dn.signum() * dn.getcoef(), dn.getexp() - Dnum::MAX_DIGITS, buf);
	}

//-------------------------------------------------------------------

static int64_t unpackCoef(const char* src, const char* lim, bool minus) {
	int64_t n = 0;
	for (; src + 1 < lim; src += 2) {
		uint16_t x = (uint8_t(src[0]) << 8) + uint8_t(src[1]);
		if (minus)
			x = ~x;
		n = n * 10000 + x;
		}
	return n;
	}

Value SuNumber::unpack(const gcstring& buf)
	{
	if (buf.size() < 2)
		return &SuNumber::zero;
	bool minus = buf[0] == PACK_MINUS;
	int s = uint8_t(buf[1]);
	if (s == 0)
		return &SuNumber::minus_infinity;
	if (s == 255)
		return &SuNumber::infinity;
	if (minus)
		s = uint8_t(~s);
	s = int8_t(s ^ 0x80);
	int sz = buf.size() - 2;
	s = -(s - sz / 2) * 4;
	int64_t n = unpackCoef(buf.ptr() + 2, buf.ptr() + buf.size(), minus);
	return new SuNumber(Dnum(minus ? -1 : +1, n, -s + Dnum::MAX_DIGITS));
	//TODO return small immediate int where possible
	}

//===================================================================

#include "func.h"
#include "interp.h"
#include "sustring.h"
#include "itostr.h"

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
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: number.Format(string)");
		auto mask = ARG(0).str();
		char* buf = (char*)_alloca(strlen(mask) + 2);
		return new SuString(format(buf, mask));
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
		return new SuNumber(dn.integer());
		}
	else if (member == FRAC)
		{
		NOARGS("number.Frac()");
		return new SuNumber(dn.frac());
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
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: number.Pow(number)");
		return from_double(pow(to_double(), ARG(0).number()->to_double()));
		}
	else if (member == HEX)
		{
		NOARGS("number.Hex()");
		char buf[40];
		u64tostr(uint32_t(dn.to_int64()), buf, 16);
		return new SuString(buf);
		}
	else if (member == OCTAL)
		{
		NOARGS("number.Octal()");
		char buf[40];
		u64tostr(uint32_t(dn.to_int64()), buf, 8);
		return new SuString(buf);
		}
	else if (member == BINARY)
		{
		NOARGS("number.Binary()");
		char buf[40];
		u64tostr(uint32_t(dn.to_int64()), buf, 2);
		return new SuString(buf);
		}
	else if (member == ROUND)
		{
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: number.Round(number)");
		return round(ARG(0).integer(), Dnum::RoundingMode::HALF_UP);
		}
	else if (member == ROUND_UP)
		{
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: number.Round(number)");
		return round(ARG(0).integer(), Dnum::RoundingMode::UP);
		}
	else if (member == ROUND_DOWN)
		{
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: number.Round(number)");
		return round(ARG(0).integer(), Dnum::RoundingMode::DOWN);
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

//===================================================================

#include "testing.h"

TEST(sudnum_format)
	{
	const char* cases[][3] = {
		{ "0"		,"###"		,"0"				},
		{ "0"		,"###."		,"0."			},
		{ "0"		,"#.##"		,".00"			},
		{ "1e-5"	,"#.##"		,".00"			},
		{ "123456"	,"###"		,"#"		},
		{ "123456"	,"###.###"	,"#"		},
		{ "123"		,"###"		,"123"		},
		{ "1"		,"###"		,"1"			},
		{ "10"		,"###"		,"10"		},
		{ "100"		,"###"		,"100"		},
		{ ".08"		,"#.##"		,".08"		},
		{ ".18"		,"#.#"		,".2"		},
		{ ".08"		,"#.#"		,".1"		},
		{ "6.789"	,"#.##"		,"6.79"			},
		{ "123"		,"##"		,"#"		},
		{ "-1"		,"#.##"		,"-"		},
		{ "-12"		,"-####"	,"-12"			},
		{ "-12"		,"(####)"	,"(12)"			},
		{ "123"		,"###,###"	,"123"		},
		{ "1234"	,"###,###"	,"1,234"		},
		{ "12345"	,"###,###"	,"12,345"		},
		{ "123456"	,"###,###"	,"123,456"	},
		{ "1e5"		,"###,###"	,"100,000"	}
		};
	char buf[32];
	for (auto c : cases)
		assert_streq(c[2], SuNumber(Dnum(c[0])).format(buf, c[1]));
	}

TEST(sudnum_packsize)
	{
	assert_eq(SuNumber(Dnum(1000)).packsize(), 4);
	assert_eq(SuNumber(Dnum(10000)).packsize(), 4);
	assert_eq(SuNumber(Dnum(10001)).packsize(), 6);
	assert_eq(SuNumber(Dnum(9999999999999999)).packsize(), 10);
	}

TEST(sudnum_pack_unpack)
	{
	char buf[32];
	const char* data[] = { "0", "inf", "-inf",
		"12", "1234", "123456", "12345678", "123456789",
		"-12", "-1234", "-123456", "-12345678", "-123456789",
		".001", "1234567890123456", "-123e-99", "456e99" };
	for (auto s : data)
		{
		SuNumber x = SuNumber(Dnum(s));
		int n = x.packsize();
		verify(n < sizeof buf);
		x.pack(buf);
		Value v = SuNumber::unpack(gcstring::noalloc(buf, n));
		assert_eq(v, Value(&x));
		}
	}
