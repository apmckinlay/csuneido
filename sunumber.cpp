// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

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

/*static*/ Value SuNumber::literal(const char* s) {
	if (s[0] == '0' && s[1] == 0)
		return SuZero;
	else if (s[0] == '1' && s[1] == 0)
		return SuOne;
	char* end;
	errno = 0;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { // hex
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

void SuNumber::out(Ostream& os) const {
	os << dn;
}

void* SuNumber::operator new(size_t n) { // NOLINT
	return ::operator new(n, noptrs);
}

static int ord = ::order("Number");

int SuNumber::order() const {
	return ord;
}

bool SuNumber::lt(const SuValue& y) const {
	if (auto yy = dynamic_cast<const SuNumber*>(&y))
		return dn < yy->dn;
	else
		return ord < y.order();
}

bool SuNumber::eq(const SuValue& y) const {
	if (auto yy = dynamic_cast<const SuNumber*>(&y))
		return dn == yy->dn;
	else
		return false;
}

char* SuNumber::format(char* buf, const char* mask) const {
	Dnum n = dn;
	int masksize = strlen(mask);
	int before = 0;
	int after = 0;
	bool intpart = true;
	for (int i = 0; i < masksize; ++i)
		switch (mask[i]) {
		case '.':
			intpart = false;
			break;
		case '#':
			if (intpart)
				++before;
			else
				++after;
			break;
		default:;
		}
	if (before + after == 0 || n.getexp() > before)
		return strcpy(buf, "#"); // too big to fit in mask

	n = n.round(after);
	char digits[Dnum::MAX_DIGITS + 1];
	int nd = n.getdigits(digits);

	int e = n.getexp();
	if (nd == 0 && after == 0) {
		digits[0] = '0';
		e = nd = 1;
	}
	int di = e - before;
	verify(di <= 0);
	char* dst = buf;
	int sign = n.signum();
	bool signok = (sign >= 0);
	bool frac = false;
	for (int mi = 0; mi < masksize; ++mi) {
		char mc = mask[mi];
		switch (mc) {
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
			[[fallthrough]];
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

size_t SuNumber::hashfn() const {
	// have to ensure that hashfn() == integer() for SHRT_MIN -> SHRT_MAX
	int n;
	if (dn.to_int(&n) && SHRT_MIN <= n && n <= SHRT_MAX)
		return n;
	return dn.hashfn();
}

short SuNumber::symnum() const {
	int n;
	if (dn.to_int(&n) && 0 <= n && n < 0x8000)
		return n;
	else
		return SuValue::symnum(); // throw
}

// throws if not convertable to int32
int SuNumber::integer() const {
	return dn.to_int32();
}

// used by type.h for dll interface
int SuNumber::trunc() const {
	return dn.integer().to_int32();
}

// packing ----------------------------------------------------------

size_t SuNumber::packsize() const {
	return dn.packsize();
}

void SuNumber::pack(char* buf) const {
	dn.pack(buf);
}

Value SuNumber::unpack(const gcstring& buf) {
	auto dnum = Dnum::unpack(buf);
	int n;
	if (dnum.to_int(&n) && SHRT_MIN <= n && n <= SHRT_MAX)
		return Value(n);
	return new SuNumber(dnum);
}

//===================================================================

#include "func.h"
#include "interp.h"
#include "sustring.h"
#include "itostr.h"

Value SuNumber::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
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
	static Value ROUND("Round");
	static Value ROUND_UP("RoundUp");
	static Value ROUND_DOWN("RoundDown");

	if (member == FORMAT) {
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: number.Format(string)");
		auto mask = ARG(0).str();
		char* buf = (char*) _alloca(strlen(mask) + 2);
		return new SuString(format(buf, mask));
	} else if (member == CHR) {
		NOARGS("number.Chr()");
		char buf[2];
		buf[0] = integer();
		buf[1] = 0;
		return new SuString(buf, 1);
	} else if (member == INT) {
		NOARGS("number.Int()");
		return new SuNumber(dn.integer());
	} else if (member == FRAC) {
		NOARGS("number.Frac()");
		return new SuNumber(dn.frac());
	} else if (member == SIN) {
		NOARGS("number.Sin()");
		return from_double(sin(to_double()));
	} else if (member == COS) {
		NOARGS("number.Cos()");
		return from_double(cos(to_double()));
	} else if (member == TAN) {
		NOARGS("number.Tan()");
		return from_double(tan(to_double()));
	} else if (member == ASIN) {
		NOARGS("number.ASin()");
		return from_double(asin(to_double()));
	} else if (member == ACOS) {
		NOARGS("number.ACos()");
		return from_double(acos(to_double()));
	} else if (member == ATAN) {
		NOARGS("number.ATan()");
		return from_double(atan(to_double()));
	} else if (member == EXP) {
		NOARGS("number.Exp()");
		return from_double(::exp(to_double()));
	} else if (member == LOG) {
		NOARGS("number.Log()");
		return from_double(log(to_double()));
	} else if (member == LOG10) {
		NOARGS("number.Log10()");
		return from_double(log10(to_double()));
	} else if (member == SQRT) {
		NOARGS("number.Sqrt()");
		return from_double(sqrt(to_double()));
	} else if (member == POW) {
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: number.Pow(number)");
		return from_double(pow(to_double(), ARG(0).number()->to_double()));
	} else if (member == HEX) {
		NOARGS("number.Hex()");
		char buf[40];
		auto n = dn.to_int64();
		if (n < INT32_MIN || UINT32_MAX < n)
			except("Hex is limited to 32 bit");
		u64tostr(uint32_t(n), buf, 16);
		return new SuString(buf);
	} else if (member == ROUND) {
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: number.Round(number)");
		return round(ARG(0).integer(), Dnum::RoundingMode::HALF_UP);
	} else if (member == ROUND_UP) {
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: number.Round(number)");
		return round(ARG(0).integer(), Dnum::RoundingMode::UP);
	} else if (member == ROUND_DOWN) {
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: number.Round(number)");
		return round(ARG(0).integer(), Dnum::RoundingMode::DOWN);
	} else {
		static UserDefinedMethods udm("Numbers");
		if (Value c = udm(member))
			return c.call(self, member, nargs, nargnames, argnames, each);
		else
			method_not_found("number", member);
	}
}

//===================================================================

#include "testing.h"

TEST(sunum_format) {
	const char* cases[][3] = {
		{"0", "###", "0"},
		{"0", "###.", "0."},
		{"0", "#.##", ".00"},
		{"1e-5", "#.##", ".00"},
		{"123456", "###", "#"},
		{"123456", "###.###", "#"},
		{"123", "###", "123"},
		{"1", "###", "1"},
		{"10", "###", "10"},
		{"100", "###", "100"},
		{".08", "#.##", ".08"},
		{".18", "#.#", ".2"},
		{".08", "#.#", ".1"},
		{"6.789", "#.##", "6.79"},
		{"123", "##", "#"},
		{"-1", "#.##", "-"},
		{"-12", "-####", "-12"},
		{"-12", "(####)", "(12)"},
		{"123", "###,###", "123"},
		{"1234", "###,###", "1,234"},
		{"12345", "###,###", "12,345"},
		{"123456", "###,###", "123,456"},
		{"1e5", "###,###", "100,000"},
	};
	char buf[32];
	for (auto c : cases)
		assert_streq(c[2], SuNumber(Dnum(c[0])).format(buf, c[1]));
}

TEST(sunum_unpack_int) {
	int data[] = {0, 1, 123, 1000, 12000, SHRT_MAX};
	for (auto i : data)
		for (int sign = +1; sign >= -1; sign -= 2) {
			int n = i * sign;
			gcstring s = packint(n);
			Value v = unpack(s);
			except_if(v.ptr() != nullptr, "should unpack to int: " << n);
			assert_eq(v.integer(), n);
		}
}
