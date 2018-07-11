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

short SuNumber::symnum() const
	{
	int n = dn.intOrMin();
	if (0 <= n && n < 0x8000)
		return n;
	else
		return SuValue::symnum(); // throw
	}

// throws if not convertable to int32
int SuNumber::integer() const
	{
	return dn.to_int32();
	}

// used by type.h for dll interface
int SuNumber::trunc() const
	{
	return dn.integer().to_int32();
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

static int pow10[] = { 1, 10, 100, 1000 };

#define E4	1'0000
#define E8	1'0000'0000

static uint64_t prev_n;
static int prev_e;
static int pack_e;
static int dig0, dig1, dig2, dig3;
static int ndigits;

static void packinfo(uint64_t n, int e)
	{
	if (n == prev_n && e == prev_e)
		return;
	prev_n = n;
	prev_e = e;

	int p = e > 0 ? 4 - (e % 4) : abs(e) % 4;
	if (p != 4)
		{
		n /= pow10[p]; // may lose up to 3 digits of precision
		e += p;
		}
	pack_e = e / 4;

	// split 8 digits (32 bits) - 8 digits (32 bits)
	// so each half can be handled as 32 bit
	// and lower precision can be handled in one half
	uint32_t hi = n / E8; // 8 digits
	uint32_t lo = n % E8; // 8 digits

	dig0 = hi / E4;
	dig1 = hi % E4;
	if (lo)
		{
		dig2 = lo / E4;
		dig3 = lo % E4;
		}
	else
		dig2 = dig3 = 0;
	ndigits =
		(dig3 != 0) ? 4 :
		(dig2 != 0) ? 3 :
		(dig1 != 0) ? 2 : 1;
	}

static int packSizeNum(uint64_t n, int e) {
	packinfo(n, e);
	return 2 /* tag and exponent */ + 2 * ndigits;
	}

size_t SuNumber::packsize() const
	{
	if (dn.isZero())
		return 1;
	if (dn.isInf())
		return 2;
	return packSizeNum(dn.getcoef(), dn.getexp());
	}

//-------------------------------------------------------------------

static int8_t scale(int e, bool minus) {
	int8_t eb = (e ^ 0x80) & 0xff;
	if (minus)
		eb = (~eb) & 0xff;
	return eb;
	}

static void packNum(bool minus, uint64_t n, int e, char* dst) {
	packinfo(n, e);
	*dst++ = scale(pack_e, minus);
	int d0 = dig0;
	int d1 = dig1;
	int d2 = dig2;
	int d3 = dig3;
	if (minus)
		{
		d0 = ~d0;
		d1 = ~d1;
		d2 = ~d2;
		d3 = ~d3;
		}
	switch (ndigits)
		{
	case 4:
		dst[7] = d3;
		dst[6] = d3 >> 8;
		[[fallthrough]];
	case 3:
		dst[5] = d2;
		dst[4] = d2 >> 8;
		[[fallthrough]];
	case 2:
		dst[3] = d1;
		dst[2] = d1 >> 8;
		[[fallthrough]];
	case 1:
		dst[1] = d0;
		dst[0] = d0 >> 8;
		break;
	default:
		;// unreachable
		}
	}

void SuNumber::pack(char* buf) const
	{
	int sign = dn.signum();
	buf[0] = sign < 0 ? PACK_MINUS : PACK_PLUS;
	if (dn.isInf())
		buf[1] = sign < 0 ? 0 : '\xff';
	else if (sign != 0)
		packNum(sign < 0, dn.getcoef(), dn.getexp(), buf + 1);
	}

//-------------------------------------------------------------------

uint64_t unpackcoef(const uint8_t* buf, int sz);

static const int MAX_TO_SHIFT = SHRT_MAX / 10000;

Value SuNumber::unpack(const gcstring& buf)
	{
	if (buf.size() < 2)
		return SuZero;
	bool minus = buf[0] == PACK_MINUS;
	int e = uint8_t(buf[1]);
	if (e == 0)
		return &SuNumber::minus_infinity;
	if (e == 255)
		return &SuNumber::infinity;
	if (minus)
		e = uint8_t(~e);
	e = int8_t(e ^ 0x80);
	e = (e - (buf.size() - 2) / 2);
	// unpack min coef for easy conversion to integer
	uint64_t n = unpackcoef((const uint8_t*)buf.ptr(), buf.size());
	if (e == 1 && n <= MAX_TO_SHIFT)
		{
		n *= 10000;
		--e;
		}
	if (e == 0 && n <= SHRT_MAX)
		return Value(minus ? -short(n) : n);
	return new SuNumber(Dnum(minus ? -1 : +1, n, 4 * e + Dnum::MAX_DIGITS));
	}

//===================================================================

#include "func.h"
#include "interp.h"
#include "sustring.h"
#include "itostr.h"

Value SuNumber::call(Value self, Value member,
	short nargs, short nargnames, short* argnames, int each)
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

TEST(sunum_format)
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

TEST(sunum_packsize)
	{
	assert_eq(SuNumber(Dnum(1000)).packsize(), 4);
	assert_eq(SuNumber(Dnum(10000)).packsize(), 4);
	assert_eq(SuNumber(Dnum(10001)).packsize(), 6);
	assert_eq(SuNumber(Dnum(9999999999999999)).packsize(), 10);
	}

static void testpack(const char* s, const char* expected, int n)
	{
	char buf[32];
	SuNumber x(s);
	memset(buf, 0x5a, sizeof buf);
	x.pack(buf);
	verify(buf[n] == 0x5a);
	verify(buf[n-1] != 0x5a);
	except_if(0 != memcmp(buf, expected, n), "pack failed: " << s);
	}
#define TESTPACK(s, expected) testpack(s, expected, sizeof (expected) - 1)

TEST(sunum_justpack)
	{
	TESTPACK("1", "\x03\x81\x00\x01");
	TESTPACK("-1", "\x02\x7e\xff\xfe");
	TESTPACK("12", "\x03\x81\x00\x0c");
	TESTPACK("123", "\x03\x81\x00\x7b");
	TESTPACK("1234", "\x03\x81\x04\xd2");
	TESTPACK("12345", "\x03\x82\x00\x01\x09\x29");
	TESTPACK(".058", "\x03\x80\x02\x44");
	}

TEST(sunum_pack_unpack)
	{
	char buf[32];
	const char* data[] = { "0", "inf", "-inf",
		"1", "10", "100", "1000", "10000",
		"12", "1234", "123456", "12345678", "123456789",
		"-12", "-1234", "-123456", "-12345678", "-123456789",
		"1234567890123456", "-123e-99", "456e99",
		".1", ".01", ".001", ".0001", ".00001" };
	for (auto s : data)
		{
		memset(buf, 0x5a, sizeof buf);
		SuNumber x = SuNumber(Dnum(s));
		int n = x.packsize();
		verify(n < sizeof buf);
		x.pack(buf);
		verify(buf[n] == 0x5a);
		verify(buf[n - 1] != 0x5a);
		Value v = SuNumber::unpack(gcstring::noalloc(buf, n));
		assert_eq(v, Value(&x));
		}
	}

TEST(sunum_unpack_int)
	{
	int data[] = { 0, 1, 123, 1000, 12000, SHRT_MAX };
	for (auto i : data)
		for (int sign = +1; sign >= -1; sign -= 2)
			{
			int n = i * sign;
			gcstring s = packint(n);
			Value v = unpack(s);
			except_if(v.ptr() != nullptr, "should unpack to int: " << n);
			assert_eq(v.integer(), n);
			}
	}

BENCHMARK(sunum_pack)
	{
	SuNumber x(Dnum("123456.78"));
	char buf[20];
	while (nreps-- > 0)
		{
		x.pack(buf);
		prev_n = 0; // clear cache
		}
	}
