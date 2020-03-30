// Copyright (c) 2018 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "dnum.h"
#include "ostreamstr.h"
#include "gcstring.h"
#include "pack.h"
#include "except.h"
#include <intrin.h> // for _BitScanForward/Reverse

/*
 * Dnum is a decimal floating point implementation
 * using a 64 bit unsigned integer for the coefficient
 * and signed 8 bit integers for the sign and exponent.
 * Precision is 16 digits.
 * Operations like multiply and divide may not be exact in last digit.
 * Max coefficient is 16 decimal digits (not full >18 digit range of 64 bit)
 * 16 digits allows splitting into two 8 digit values which fit in 32 bit.
 * Assumed decimal point is to the left of the coefficient.
 * i.e. if exponent is 0, then 0 <= value < 1
 * Zero is represented with a sign of 0 (Zeroed value is zero)
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

#if NDEBUG
#define CHECK(cond)
#else
#define CHECK(cond) verify(cond)
#endif

typedef struct {
	uint32_t lo32;
	uint32_t hi32;
} Int64;
#define LO32(n) (*reinterpret_cast<Int64*>(&(n))).lo32
#define HI32(n) (*reinterpret_cast<Int64*>(&(n))).hi32

// sign values, zero = 0
enum { NEG_INF = -2, NEG = -1, POS = +1, POS_INF = +2 };

#define COEF_MIN 1000'0000'0000'0000ull
#define COEF_MAX 9999'9999'9999'9999ull
#define MAX_DIGITS 16
#define MAX_SHIFT (MAX_DIGITS - 1)

#define E1 10
#define E2 100
#define E3 1000
#define E4 10'000
#define E5 100'000
#define E6 1'000'000
#define E7 10'000'000
#define E8 100'000'000

Dnum Dnum::inf(int sign) {
	Dnum dn;
	dn.sign = sign < 0 ? NEG_INF : POS_INF;
	dn.coef = 1;
	return dn;
}

const Dnum Dnum::ZERO;
const Dnum Dnum::ONE(1);
const Dnum Dnum::MINUS_ONE(-1);
// infinite coefficient isn't used, just has to be non-zero for constructor
const Dnum Dnum::INF = inf(+1);
const Dnum Dnum::MINUS_INF = inf(-1);
const Dnum Dnum::MAX_INT{POS, COEF_MAX, 16};

int clz64(uint64_t n) {
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
	while (n != 0) {
		n >>= 1; // shift right assuming more leading zeros
		++c;
	}
	return 64 - c;
#endif
}

namespace {
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
	10000000000000000000ull,
};
uint64_t halfpow10[20] = {
	// for rounding
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
	5000000000000000000ull,
};

// returns 0 to 20
int ilog10(uint64_t x) {
	// based on Hacker's Delight, single 64 bit compare
	if (x == 0)
		return 0;
	int y = (19 * (63 - clz64(x))) >> 6;
	if (y < 19 && x >= pow10[y + 1])
		++y;
	return y;
}

// the maximum we can safely shift left (*10)
int maxShift(uint64_t x) {
	int i = ilog10(x);
	return i > MAX_SHIFT ? 0 : MAX_SHIFT - i;
}

// "shift" coef "left" as far as possible, returns amount shifted
int maxCoef(uint64_t& coef) {
	int i = maxShift(coef);
	coef *= pow10[i];
	CHECK(COEF_MIN <= coef && coef <= COEF_MAX);
	return i;
}
} // end of namespace

#define SIGN(n) ((n) < 0 ? -1 : (n) > 0 ? +1 : 0)

#define SETINF() \
	do { \
		sign = s < 0 ? NEG_INF : POS_INF; \
		coef = 1; \
		exp = 0; \
	} while (false)

Dnum::Dnum(int s, uint64_t c, int e) {
	if (s == 0 || c == 0 || e < INT8_MIN)
		*this = Dnum::ZERO;
	else if (s == POS_INF || s == NEG_INF)
		SETINF();
	else {
		bool atmax = false;
		while (c > COEF_MAX) {
			c = (c + 5) / 10; // drop/round least significant digit
			++e;
			atmax = true;
		}
		if (!atmax)
			e -= maxCoef(c);
		if (e > INT8_MAX)
			SETINF();
		else {
			sign = SIGN(s);
			coef = c;
			exp = e;
			CHECK(COEF_MIN <= coef && coef <= COEF_MAX);
		}
	}
}

Dnum::Dnum(int n)
	: coef(n < 0 ? -int64_t(n) : n), sign(n < 0 ? NEG : n > 0 ? POS : 0) {
	if (coef == 0)
		exp = 0;
	else
		exp = MAX_DIGITS - maxCoef(coef);
}

Dnum::Dnum(int64_t n) {
	if (n == 0)
		*this = ZERO;
	else {
		if (n > 0)
			sign = POS;
		else {
			n = -n;
			sign = NEG;
		}
		int p = maxShift(n);
		coef = n * pow10[p];
		exp = MAX_DIGITS - p;
		CHECK(COEF_MIN <= coef && coef <= COEF_MAX);
	}
}

namespace {
bool match(const char*& s, char c) {
	if (*s != c)
		return false;
	++s;
	return true;
}

bool isdigit(char c) {
	return '0' <= c && c <= '9';
}

// returns coef (maximized), advances s, sets exp
uint64_t get_coef(const char*& s, int& exp) {
	bool digits = false;
	bool before_decimal = true;

	// skip leading zeroes, no effect on result
	for (; *s == '0'; ++s)
		digits = true;
	if (*s == '.' && s[1])
		digits = false;

	uint64_t n = 0;
	int p = MAX_SHIFT;
	while (true) {
		if (isdigit(*s)) {
			digits = true;
			// ignore extra decimal places
			if (*s != '0' && p >= 0)
				n += (*s - '0') * pow10[p];
			--p;
			++s;
		} else if (before_decimal) {
			// decimal point or end
			exp = MAX_SHIFT - p;
			if (*s != '.')
				break;
			++s;
			before_decimal = false;
			if (!digits)
				for (; *s == '0'; ++s, --exp)
					digits = true;
		} else
			break;
	}
	if (!digits)
		except("numbers require at least one digit");
	return n;
}

int get_exp(const char*& s) {
	int e = 0;
	if (match(s, 'e') || match(s, 'E')) {
		int esign = match(s, '-') ? -1 : 1;
		match(s, '+');
		for (; isdigit(*s); ++s)
			e = e * 10 + (*s - '0');
		e *= esign;
	}
	return e;
}
} // namespace

// accepts "inf", "+inf", and "-inf"
// throws for invalid
// result is left aligned with maximum coef
// returns INF if exponent too large, ZERO if exponent too small
Dnum::Dnum(const char* s) {
	sign = POS;
	if (match(s, '-'))
		sign = NEG;
	else
		match(s, '+');
	if (0 == strcmp(s, "inf")) {
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
	else {
		exp = e;
		CHECK(COEF_MIN <= coef && coef <= COEF_MAX);
	}
}

namespace {
char* append(char* dst, char* src) {
	while (*src)
		*dst++ = *src++;
	return dst;
}
} // namespace

#define PUT(n, p) \
	*d++ = '0' + ((n) / (p)); \
	(n) %= (p)

int Dnum::getdigits(char* buf) const {
	char* d = buf;
	uint32_t hi = coef / E8; // 8 digits
	uint32_t lo = coef % E8; // 8 digits
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
	while (d > buf && d[-1] == '0')
		--d;
	*d = 0;
	return d - buf;
}

inline const int MAX_LEADING_ZEROS = 7;

char* Dnum::tostr(char* buf, int len) const {
	verify(len >= STRLEN);
	char* dst = buf;
	if (isZero())
		return strcpy(dst, "0");
	if (sign < 0)
		*dst++ = '-';
	if (isInf()) {
		strcpy(dst, "inf");
		return buf;
	}

	char digits[17];
	int ndigits = getdigits(digits);
	int e = exp - ndigits;
	if (-MAX_LEADING_ZEROS <= exp && exp <= 0) {
		// decimal to the left
		*dst++ = '.';
		for (int n = -e - ndigits; n > 0; --n)
			*dst++ = '0';
		dst = append(dst, digits);
	} else if (-ndigits < e && e <= -1) {
		// decimal within
		int dec = ndigits + e;
		for (int i = 0; digits[i]; ++i) {
			if (i == dec)
				*dst++ = '.';
			*dst++ = digits[i];
		}
	} else if (0 < exp && exp <= MAX_DIGITS) {
		// decimal to the right
		dst = append(dst, digits);
		for (int i = 0; i < e; ++i)
			*dst++ = '0';
	} else {
		// scientific notation
		*dst++ = digits[0];
		if (digits[1]) {
			*dst++ = '.';
			dst = append(dst, digits + 1);
		}
		*dst++ = 'e';
		e += strlen(digits) - 1;
		if (e < 0) {
			*dst++ = '-';
			e = -e;
		}
		int nd = e >= 100 ? 3 : e > 10 ? 2 : 1;
		switch (nd) {
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

Ostream& operator<<(Ostream& os, const Dnum& dn) {
	char buf[Dnum::STRLEN];
	return os << dn.tostr(buf, sizeof buf);
}

// "raw" format for test and debug
gcstring Dnum::show() const {
	OstreamStr os(32);
	switch (sign) {
	case NEG_INF:
		os << "--";
		break;
	case NEG:
		os << "-";
		break;
	case 0:
		os << "z";
		break;
	case POS:
		os << "+";
		break;
	case POS_INF:
		os << "++";
		break;
	default:
		os << "?";
		break;
	}
	if (coef == 0)
		os << '0';
	else {
		os << ".";
		uint64_t c = coef;
		for (int i = MAX_SHIFT; i >= 0 && c != 0; --i) {
			auto p = pow10[i];
			int digit = int(c / p);
			c %= p;
			CHECK(0 <= digit && digit <= 9);
			os << char('0' + digit);
		}
	}
	os << 'e' << exp;
	return os.gcstr();
}

gcstring Dnum::to_gcstr() const {
	OstreamStr os(32);
	os << *this;
	return os.gcstr();
}

bool Dnum::isInf() const {
	return sign == POS_INF || sign == NEG_INF;
}

// operations -------------------------------------------------------

bool operator==(const Dnum& x, const Dnum& y) {
	// WARNING: assumes/requires both x and y are normalized
	if (x.sign != y.sign)
		return false;
	if (x.sign == 0 || x.sign == NEG_INF || x.sign == POS_INF)
		return true;
	return x.exp == y.exp && x.coef == y.coef;
}

// unary minus
Dnum Dnum::operator-() const {
	// don't need normalization
	Dnum n(*this);
	n.sign *= -1;
	return n;
}

Dnum Dnum::abs() const {
	// don't need normalization
	if (sign >= 0)
		return *this;
	Dnum n(*this);
	n.sign = -n.sign;
	return n;
}

#define CMP(x, y) ((x) < (y) ? -1 : (x) > (y) ? +1 : 0)

// returns -1 for less than, 0 for equal, +1 for greater than
int Dnum::cmp(const Dnum& x, const Dnum& y) {
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

Dnum operator*(const Dnum& x, const Dnum& y) {
	int sign = x.sign * y.sign;
	if (sign == 0)
		return Dnum::ZERO;
	if (x.isInf() || y.isInf())
		return Dnum::inf(sign);
	int e = x.exp + y.exp;

	// split unevenly to use full 64 bit range to get more precision
	// and avoid needing xlo * ylo
	uint64_t xhi = x.coef / E7; // 9 digits
	uint32_t xlo = x.coef % E7; // 7 digits
	uint64_t yhi = y.coef / E7; // 9 digits
	uint32_t ylo = y.coef % E7; // 7 digits

	uint64_t c = xhi * yhi;
	if (xlo || ylo)
		c += (xlo * yhi + ylo * xhi) / E7;
	return Dnum(sign, c, e - 2);
}

// divide -----------------------------------------------------------

uint64_t div128(uint64_t, uint64_t);

Dnum operator/(const Dnum& x, const Dnum& y) {
	int sign = x.sign * y.sign;
	if (sign == 0)
		return x.isZero() ? Dnum::ZERO : /* y.isZero() */ Dnum::inf(x.sign);
	if (x.isInf()) {
		if (y.isInf())
			return sign < 0 ? Dnum::MINUS_ONE : Dnum::ONE;
		return Dnum::inf(sign);
	}
	if (y.isInf())
		return Dnum::ZERO;

	uint64_t q = div128(x.coef, y.coef);
	return Dnum(sign, q, x.exp - y.exp);
}

// add/sub ----------------------------------------------------------

Dnum operator+(const Dnum& x, const Dnum& y) {
	if (x.sign == 0)
		return y;
	if (y.sign == 0)
		return x;
	if (x.sign == POS_INF || x.sign == NEG_INF)
		return (y.sign == -x.sign) ? Dnum::ZERO : x;
	if (y.sign == POS_INF || y.sign == NEG_INF)
		return y;

	Dnum yy(y), xx(x);
	if (xx.exp != yy.exp)
		if (!align(xx, yy))
			return xx; // result is larger if exponents too far apart to align
	return xx.sign == yy.sign ? uadd(xx, yy) : usub(xx, yy);
}

// returns whether or not it was able to align, may swap to ensure x is larger
bool align(Dnum& x, Dnum& y) {
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

// x and y must be aligned
Dnum uadd(const Dnum& x, const Dnum& y) {
	// won't overflow 64 bit since we're only using 16 digits
	return Dnum(x.sign, x.coef + y.coef, x.exp);
}

// x and y must be aligned
Dnum usub(const Dnum& x, const Dnum& y) {
	return x.coef > y.coef ? Dnum(x.sign, x.coef - y.coef, x.exp)
						   : Dnum(-x.sign, y.coef - x.coef, x.exp);
}

// packing ----------------------------------------------------------

namespace {
uint64_t prev_coef = 0;
int prev_size = 0;
// 8 bytes is sufficient for 16 decimal digits, 2 digits per byte
// WARNING: static so not threadsafe, must not yield while in use
uint8_t bytes[8];

int coef_to_bytes(uint64_t coef) {
	// cache the last result, to optimize the common case of packsize,pack
	if (coef == prev_coef)
		return prev_size;
	prev_coef = coef;

	// split 8 digits (32 bits) - 8 digits (32 bits)
	// so each half can be handled as 32 bit
	// and lower precision can be handled in one half
	uint32_t hi = coef / E8; // 8 digits
	uint32_t lo = coef % E8; // 8 digits

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
	else {
		n = 8;
		bytes[4] = lo / E6;
		lo %= E6;
		bytes[5] = lo / E4;
		lo %= E4;
		bytes[6] = lo / E2;
		bytes[7] = lo % E2; // least significant
	}
	while (bytes[n - 1] == 0)
		--n;
	return prev_size = n;
}
} // end of namespace

size_t Dnum::packsize() const {
	if (sign == 0)
		return 1; // just tag
	if (sign == NEG_INF || sign == POS_INF)
		return 3;
	return 2 + coef_to_bytes(coef);
}

void Dnum::pack(char* dst) const {
	*dst++ = sign < 0 ? PACK_MINUS : PACK_PLUS;
	if (sign == 0)
		return;
	if (sign == NEG_INF) {
		dst[0] = dst[1] = 0;
		return;
	}
	if (sign == POS_INF) {
		dst[0] = dst[1] = uint8_t(0xff);
		return;
	}
	uint8_t xor = sign < 0 ? 0xff : 0;
	auto e = exp ^ 0x80; // convert to sort as unsigned
	*dst++ = e ^ xor;

	int n = coef_to_bytes(coef);
	uint8_t* b = bytes + n;
	dst += n;
	switch (n) {
	case 8:
		*--dst = *--b ^ xor;
		[[fallthrough]];
	case 7:
		*--dst = *--b ^ xor;
		[[fallthrough]];
	case 6:
		*--dst = *--b ^ xor;
		[[fallthrough]];
	case 5:
		*--dst = *--b ^ xor;
		[[fallthrough]];
	case 4:
		*--dst = *--b ^ xor;
		[[fallthrough]];
	case 3:
		*--dst = *--b ^ xor;
		[[fallthrough]];
	case 2:
		*--dst = *--b ^ xor;
		[[fallthrough]];
	case 1:
		*--dst = *--b ^ xor;
		break;
	default:
		unreachable();
	}
}

Dnum Dnum::unpack(const gcstring& s) {
	auto src = reinterpret_cast<const uint8_t*>(s.ptr());
	if (s.size() == 1) {
		CHECK(src[0] == PACK_PLUS);
		return ZERO;
	}
	int sign;
	uint8_t xor ;
	switch (*src++) {
	case PACK_MINUS:
		sign = -1;
		xor = 0xff;
		break;
	case PACK_PLUS:
		sign = +1;
		xor = 0;
		break;
	default:
		unreachable();
	}

	int8_t exp = *src++ ^ 0x80 ^ xor;

	uint8_t b = *src ^ xor;
	if (b == 0xff)
		return sign < 0 ? MINUS_INF : INF;

	CHECK(s.size() > 2);
	// work in two 32 bit halves to reduce 64 bit operations
	uint32_t hi = 0;
	uint32_t lo = 0;
	switch (s.size() - 2) {
	case 8:
		lo += (src[7] ^ xor);
		[[fallthrough]];
	case 7:
		lo += (src[6] ^ xor) * E2;
		[[fallthrough]];
	case 6:
		lo += (src[5] ^ xor) * E4;
		[[fallthrough]];
	case 5:
		lo += (src[4] ^ xor) * E6;
		[[fallthrough]];
	case 4:
		hi += (src[3] ^ xor);
		[[fallthrough]];
	case 3:
		hi += (src[2] ^ xor) * E2;
		[[fallthrough]];
	case 2:
		hi += (src[1] ^ xor) * E4;
		[[fallthrough]];
	case 1:
		hi += (src[0] ^ xor) * E6;
		break;
	default:
		unreachable();
	}
	uint64_t coef = uint64_t(hi) * E8 + lo;
	return Dnum(sign, coef, exp);
}

// int packing ------------------------------------------------------
// could have specialized 32 bit code rather than via 64 bit Dnum

size_t packsizeint(int n) {
	return Dnum(n).packsize();
}

void packint(char* dst, int n) {
	return Dnum(n).pack(dst);
}

int unpackint(const gcstring& s) {
	int n;
	verify(Dnum::unpack(s).to_int(&n));
	return n;
}

// end of packing ---------------------------------------------------

static double dpow10[] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
	1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21,
	1e22, 1e23, 1e24, 1e25, 1e26, 1e27, 1e28, 1e29, 1e30, 1e31, 1e32, 1e33,
	1e34, 1e35, 1e36, 1e37, 1e38, 1e39, 1e40, 1e41, 1e42, 1e43, 1e44, 1e45,
	1e46, 1e47, 1e48, 1e49, 1e50, 1e51, 1e52, 1e53, 1e54, 1e55, 1e56, 1e57,
	1e58, 1e59, 1e60, 1e61, 1e62, 1e63, 1e64, 1e65, 1e66, 1e67, 1e68, 1e69,
	1e70, 1e71, 1e72, 1e73, 1e74, 1e75, 1e76, 1e77, 1e78, 1e79, 1e80, 1e81,
	1e82, 1e83, 1e84, 1e85, 1e86, 1e87, 1e88, 1e89, 1e90, 1e91, 1e92, 1e93,
	1e94, 1e95, 1e96, 1e97, 1e98, 1e99, 1e100, 1e101, 1e102, 1e103, 1e104,
	1e105, 1e106, 1e107, 1e108, 1e109, 1e110, 1e111, 1e112, 1e113, 1e114, 1e115,
	1e116, 1e117, 1e118, 1e119, 1e120, 1e121, 1e122, 1e123, 1e124, 1e125, 1e126,
	1e127, 1e128, 1e129, 1e130, 1e131, 1e132, 1e133, 1e134, 1e135, 1e136, 1e137,
	1e138, 1e139, 1e140, 1e141, 1e142, 1e143, 1e144};

double Dnum::to_double() const {
	switch (sign) {
	case 0:
		return 0.0;
	case NEG_INF:
		return -INFINITY;
	case POS_INF:
		return INFINITY;
	default:
		int e = exp - MAX_DIGITS;
		return sign * (e < 0 ? coef / dpow10[-e] : coef * dpow10[e]);
	}
}

Dnum Dnum::from_double(double d) { // needed for math/trig functions
	if (isnan(d))
		except("can't convert NaN to number");
	if (isinf(d))
		return d < 0 ? MINUS_INF : INF;
	// probably not the most efficient, but simple
	char buf[_CVTBUFSIZE];
	_gcvt(d, MAX_DIGITS, buf);
	return Dnum(buf);
}

Dnum Dnum::round(int r, RoundingMode mode) const {
	if (isZero() || isInf() || r >= MAX_DIGITS)
		return *this;
	if (r <= -MAX_DIGITS)
		return ZERO;
	Dnum n(sign, coef, exp + r); // multiply by 10^r
	n = n.integer(mode);
	if (n.sign == POS || n.sign == NEG) // i.e. not zero or inf
		return Dnum(n.sign, n.coef, n.exp - r);
	return n;
}

Dnum Dnum::integer(RoundingMode mode) const {
	if (sign == 0 || sign == NEG_INF || sign == POS_INF || exp >= MAX_DIGITS)
		return *this;
	if (exp <= 0) {
		if (mode == RoundingMode::UP ||
			(mode == RoundingMode::HALF_UP && exp == 0 && coef >= ONE.coef * 5))
			return Dnum(sign, ONE.coef, exp + 1);
		return ZERO;
	}
	int e = MAX_DIGITS - exp;
	uint64_t frac = coef % pow10[e];
	if (frac == 0)
		return *this;
	uint64_t i = coef - frac;
	if ((mode == RoundingMode::UP && frac > 0) ||
		(mode == RoundingMode::HALF_UP && frac >= halfpow10[e]))
		return Dnum(sign, i + pow10[e], exp); // normalize
	return Dnum(sign, i, exp);                // TODO doesn't need to normalize
}

Dnum Dnum::frac() const {
	if (sign == 0 || sign == NEG_INF || sign == POS_INF || exp >= MAX_DIGITS)
		return ZERO;
	if (exp <= 0)
		return *this;
	int64_t frac = coef % pow10[MAX_DIGITS - exp];
	return frac == coef ? *this : Dnum(sign, frac, exp);
}

// Return an int32 if the Dnum can be losslessly converted, else throw
int32_t Dnum::to_int32() const {
	int64_t n = to_int64();
	if (INT32_MIN <= n && n <= INT32_MAX)
		return int32_t(n);
	except("can't convert number to integer " << *this);
}

// Return an int64 if the Dnum can be losslessly converted, else throw
int64_t Dnum::to_int64() const {
	if (sign == 0)
		return 0;
	if (sign != NEG_INF && sign != POS_INF) {
		if (0 < exp && exp < MAX_DIGITS &&
			(coef % pow10[MAX_DIGITS - exp]) == 0)
			return sign * (coef / pow10[MAX_DIGITS - exp]); // usual case
		if (exp == MAX_DIGITS)
			return sign * coef;
		if (exp == MAX_DIGITS + 1)
			return sign * (coef * 10);
		if (exp == MAX_DIGITS + 2)
			return sign * (coef * 100);
		if (exp == MAX_DIGITS + 3 && coef < INT64_MAX / 1000)
			return sign * (coef * 1000);
	}
	except("can't convert number to integer " << *this);
}

// Return true and set *pn if the Dnum can be losslessly converted, else false
bool Dnum::to_int(int* pn) const {
	if (sign == 0) {
		*pn = 0;
		return true;
	}
	if (sign != NEG_INF && sign != POS_INF && 0 < exp && exp <= 10 &&
		(coef % pow10[MAX_DIGITS - exp]) == 0) {
		int64_t n = sign * (coef / pow10[MAX_DIGITS - exp]);
		if (INT_MIN < n && n <= INT_MAX) {
			*pn = n;
			return true;
		}
	}
	return false;
}

size_t Dnum::hashfn() const {
	const size_t prime = 31;
	size_t result = 1;
	result = prime * result + (coef ^ (coef >> 32));
	result = prime * result + exp;
	result = prime * result + sign;
	return result;
}

// tests ------------------------------------------------------------

#include "testing.h"
#include "porttest.h"
#include "list.h"
#include <utility>
using namespace std::rel_ops;

#define TRUNC(n) ((n) / 10)
#define ROUND(n) (((n) + 5) / 10)

static void parse(const char* s, const char* expected) {
	gcstring ns = Dnum(s).show();
	except_if(ns != expected,
		"parse " << s << " got " << ns << " expected " << expected);
}
TEST(dnum_constructors) {
	// int
	assert_eq(Dnum::ZERO.show(), "z0e0");
	assert_eq(Dnum::ONE.show(), "+.1e1");
	assert_eq(Dnum(0), Dnum::ZERO);
	assert_eq(Dnum(1234).show(), "+.1234e4");
	assert_eq(Dnum(INT_MIN).to_int32(), INT_MIN);

	// string
	xassert(Dnum("."));
	xassert(Dnum("1.2.3"));
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
	parse(".1234567890123456789", "+.1234567890123456e0");
	parse("11111111111111111111", "+.1111111111111111e20");
	parse(".001", "+.1e-2");
	parse("0.001", "+.1e-2");
	parse(".000000000000000000001", "+.1e-20");
	parse("0.000000000000000000001", "+.1e-20");

	assert_eq(Dnum("1e999"), Dnum::INF);
	assert_eq(Dnum("1e-999"), Dnum::ZERO);
	assert_eq(Dnum("0e999"), Dnum::ZERO);
}

static void str(const Dnum dn, const char* expected) {
	char buf[Dnum::STRLEN];
	(void) dn.tostr(buf, sizeof buf);
	except_if(0 != strcmp(buf, expected),
		"tostr\n"
			<< buf << " result\n"
			<< expected << " expected");
}
static void str(const char* s) {
	Dnum n(s);
	str(n, s);
}
TEST(dnum_tostr) {
	str(Dnum::ZERO, "0");
	str(Dnum::ONE, "1");
	str(Dnum::INF, "inf");
	str(Dnum::MINUS_INF, "-inf");
	str(Dnum(1234), "1234");
	str(Dnum(-1234), "-1234");
	str(Dnum("1e15"), "1000000000000000");
	str(Dnum("1.23e9"), "1230000000");
	str(Dnum("1e100"), "1e100");
	str(Dnum("6.545631232121333e100"), "6.545631232121333e100");
	str("1e16");
	str("1.23e16");
	str("-123.456");
	str(".000123");
}

TEST(dnum_misc) {
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
	assert_eq(Dnum(POS, 1, 999), Dnum::INF);       // exponent overflow
	assert_eq(Dnum(NEG, 1, 999), Dnum::MINUS_INF); // exponent overflow
	assert_eq(Dnum(POS, 1, -999), Dnum::ZERO);     // exponent underflow
	assert_eq(Dnum(NEG, 1, -999), Dnum::ZERO);     // exponent underflow
	assert_eq(Dnum("1e127"), Dnum::INF);           // exponent overflow

	// unary minus
	assert_eq((-Dnum("0")).show(), "z0e0");
	assert_eq((-Dnum("+123")).to_gcstr(), "-123");
	assert_eq((-Dnum("-123")).to_gcstr(), "123");

	// abs
	assert_eq(Dnum("0").abs().show(), "z0e0");
	assert_eq(Dnum("123").abs().to_gcstr(), "123");
	assert_eq(Dnum("-123").abs().to_gcstr(), "123");
}

template <typename X, typename Y>
static void cmp(X x_, Y y_, int c) {
	Dnum x(x_);
	Dnum y(y_);
	except_if(Dnum::cmp(x, y) != (c),
		(x) << " <=> " << (y) << " = " << Dnum::cmp(x, y));
}
TEST(dnum_cmp) {
	Dnum::cmp(Dnum("123.4"), Dnum("123.45"));
	const char* nums[] = {"-inf", "-1e9", "-123.45", "-123", "-100", "-1e-9",
		"0", "1e-9", "100", "123", "123.45", "1e9", "inf"};
	int i = 0;
	for (auto x : nums) {
		int j = 0;
		for (auto y : nums) {
			cmp(x, y, CMP(i, j));
			cmp(y, x, CMP(j, i));
			++j;
		}
		++i;
	}
}

template <typename X, typename Y, typename E>
static void mul(X x, Y y, E expected) {
	Dnum dx(x);
	Dnum dy(y);
	Dnum e(expected);
	Dnum p = dx * dy;
	except_if(p != e,
		dx << " * " << dy << "\n"
		   << p << " result\n"
		   << e << " expected");
	p = dy * dx;
	except_if(p != e,
		dy << " * " << dx << "\n"
		   << p << " result\n"
		   << e << " expected");
}
TEST(dnum_mul) {
	// special cases (no actual math)
	mul(0, 0, 0);
	mul(0, 123, 0);
	mul(0, "inf", 0);
	mul("inf", 123, "inf");
	mul("inf", "inf", "inf");

	// fast, single multiply
	const int nums[] = {0, 1, -1, 100, 1234, 9999, -1234};
	for (auto x : nums)
		for (auto y : nums)
			mul(x, y, x * y);
	Dnum z("4294967295");
	mul(z, z, "1844674406511962e4");

	mul("112233445566", "112233445566", "1259634630361628e7");

	mul("1111111111111111", "1111111111111111", "1.234567901234568e30");
	mul("100000001", "100000001", "100000002e8");
	mul("1000000001", "1000000001", "1000000002e9");
	mul("123456789", "123456789", "1524157875019052e1");
	mul("1234567899", "1234567899", "1524157897241274e3");
	mul(".9999999999999999", ".9999999999999999", ".9999999999999998");

	mul(Dnum::MAX_INT, Dnum::MAX_INT, "9.999999999999998e31");
	mul(Dnum::MAX_INT, Dnum::ONE, Dnum::MAX_INT);
	mul(Dnum::MAX_INT, "1111111111111111", "1.111111111111111e31");

	mul("2e99", "2e99", "inf"); // exp overflow
}

static void div(const char* x, const char* y, const char* expected) {
	Dnum q = Dnum(x) / Dnum(y);
	Dnum e(expected);
	except_if(q != e,
		x << " / " << y << "\n"
		  << q << " result\n"
		  << e << " expected");
}
TEST(dnum_div) {
	// special cases (no actual math)
	div("0", "0", "0");
	div("123", "0", "inf");
	div("123", "1", "123");
	div("123", "10", "12.3");
	div("123456", "1e3", "123.456");
	div("123", "inf", "0");
	div("inf", "123", "inf");
	div("inf", "inf", "1");

	// divides evenly
	div("123", "123", "1");
	div("4444", "2222", "2");
	div("2222", "4444", ".5");
	div("1", "16", ".0625");
	div("123000", ".000123", "1e9");

	// long division
	div("1", "3", ".3333333333333333");
	div("2", "3", ".6666666666666666");
	div("11", "17", ".6470588235294117");
	div("1234567890123456", "9876543210987654", ".1249999988609374");
	div("1", "3333333333333333", "3e-16");
	div("12", ".4444444444444444", "27");
	div(".9999999999999999", ".9999999999999999", "1");

	// exp overflow
	div("1e99", "1e-99", "inf");
	div("1e-99", "1e99", "0");
}

template <typename X, typename Y, typename Z>
static void addsub(X x_, Y y_, Z sum_) {
	Dnum x(x_);
	Dnum y(y_);
	Dnum sum(sum_);
	Dnum z = x + y; // add
	except_if(z != sum,
		x << " + " << y << "\n"
		  << z << " result\n"
		  << sum << " expected");
	z = y + x; // add reverse
	except_if(z != sum,
		y << " + " << x << "\n"
		  << z << " result\n"
		  << sum << " expected");
	z = -x + -y; // add negatives
	except_if(z != -sum,
		-x << " + " << -y << "\n"
		   << z << " result\n"
		   << -sum << " expected");
	z = sum - y; // subtract
	except_if(z != x,
		sum << " - " << y << "\n"
			<< z << " result\n"
			<< x << " expected");
}
TEST(dnum_addsub) {
	// special cases
	addsub(123, 0, 123);
	addsub("inf", "-inf", 0);
	addsub("inf", 123, "inf");
	addsub("-inf", 123, "-inf");
	assert_eq(Dnum::INF + Dnum::INF, Dnum::INF);
	assert_eq(Dnum::MINUS_INF + Dnum::MINUS_INF, Dnum::MINUS_INF);
	assert_eq(Dnum::INF - Dnum::INF, Dnum::ZERO);
	assert_eq(Dnum::MINUS_INF - Dnum::MINUS_INF, Dnum::ZERO);

	const int nums[] = {0, 1, 12, 99, 100, 123, 1000, 1234, 9999};
	for (auto x : nums)
		for (auto y : nums) {
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

TEST(dnum_packing) {
	const char* nums[] = {"-inf", "-1e9", "-123.45", "-123", "-100", "-1e-9",
		"0", "1e-9", ".123", "100", "123", "123.45", "98765432",
		"98765432.12345678", "1e9", "inf"};
	List<gcstring> packed;
	for (auto s : nums) {
		Dnum dn(s);
		int n = dn.packsize();
		verify(n < 20);
		char buf[20];
		dn.pack(buf);
		gcstring p(buf, n);
		Dnum d2 = Dnum::unpack(p);
		assert_eq(d2, dn);
		packed.add(p);
	}
	for (int i = 0; i < packed.size(); ++i)
		for (int j = 0; j < packed.size(); ++j)
			except_if(CMP(packed[i], packed[j]) != CMP(i, j),
				"packed " << nums[i] << " <=> " << nums[j]);
}

TEST(dnum_to_double) {
	const char* exact[] = {"0", "1", "-1", "1e9", "123456", "1234567890123456"};
	for (auto s : exact) {
		Dnum x(s);
		double n = strtod(s, nullptr);
		assert_eq(n, x.to_double());
		assert_eq(n, x.to_double());
	}
	const char* approx[] = {".1", "-1.23e99", "4.56e-99"};
	for (auto s : approx) {
		double x = Dnum(s).to_double();
		double y = strtod(s, nullptr);
		verify(abs(1 - x / y) < 1e-15);
	}
}

TEST(dnum_from_double) {
	assert_eq(Dnum::from_double(-INFINITY), Dnum::MINUS_INF);
	assert_eq(Dnum::from_double(INFINITY), Dnum::INF);
	assert_eq(Dnum::from_double(0.0), Dnum::ZERO);
	assert_eq(Dnum::from_double(123.456e9), Dnum("123.456e9"));
	assert_eq(
		Dnum::from_double(0.37161106994968846), Dnum("0.3716110699496885"));
	assert_eq(
		Dnum::from_double(37161106994968846.0), Dnum("37161106994968850"));
}

TEST(dnum_integer) {
	const char* same[] = {"0", "123", "123e9", "12e34"};
	for (auto s : same) {
		Dnum n(s);
		assert_eq(n.integer(), n);
	}
	const char* data[][2] = {{".123", "0"}, {"123.456", "123"}, {"1e-99", "0"}};
	for (auto d : data)
		assert_eq(Dnum(d[0]).integer(), Dnum(d[1]));
}

TEST(dnum_round) {
	auto UP = Dnum::RoundingMode::UP;
	auto DOWN = Dnum::RoundingMode::DOWN;
	auto HALF_UP = Dnum::RoundingMode::HALF_UP;
	using Mode = Dnum::RoundingMode;

	Dnum data[] = {Dnum::ZERO, Dnum::INF, Dnum::MINUS_INF};
	Mode modes[] = {DOWN, HALF_UP, UP};
	int digits[] = {2, 0, -2};
	for (Dnum n : data)
		for (Mode mode : modes)
			for (int d : digits)
				assert_eq(n.round(d, mode), n);

	Dnum n("1256.5634");
	for (Mode mode : modes) {
		assert_eq(n.round(99, mode), n);
		assert_eq(n.round(-99, mode), Dnum::ZERO);
	}

	assert_eq(n.round(2, DOWN), Dnum("1256.56"));
	assert_eq(n.round(2, HALF_UP), Dnum("1256.56"));
	assert_eq(n.round(2, UP), Dnum("1256.57"));

	assert_eq(n.round(0, DOWN), Dnum("1256"));
	assert_eq(n.round(0, HALF_UP), Dnum("1257"));
	assert_eq(n.round(0, UP), Dnum("1257"));

	assert_eq(n.round(-2, DOWN), Dnum("1200"));
	assert_eq(n.round(-2, HALF_UP), Dnum("1300"));
	assert_eq(n.round(-2, UP), Dnum("1300"));

	assert_eq(Dnum(".08").round(1, HALF_UP), Dnum(".1"));
	assert_eq(Dnum(".08").round(0, HALF_UP), Dnum::ZERO);
}

TEST(to_int64) {
	int64_t data[] = {0, 1, -1, 123, -123, INT_MAX, INT_MIN,
		1234'5678'9012'3456L, -9999'9999'9999'9999L};
	for (auto n : data)
		assert_eq(Dnum(n).to_int64(), n);

	assert_eq(Dnum(".99e17").to_int64(), 99000000000000000L);
	assert_eq(Dnum("-.99e17").to_int64(), -99000000000000000L);
	assert_eq(Dnum(".99e18").to_int64(), 990000000000000000L);
	assert_eq(Dnum(".91e19").to_int64(), 9100000000000000000L);

	xassert(Dnum(".99e19").to_int64());
	xassert(Dnum("1e20").to_int64());
	xassert(Dnum("1.5").to_int64());
}

TEST(intOrMin) {
	int n;
	verify(Dnum::ZERO.to_int(&n) && n == 0);
	verify(Dnum(123).to_int(&n) && n == 123);
	verify(Dnum(-123).to_int(&n) && n == -123);
	verify(Dnum(INT_MAX).to_int(&n) && n == INT_MAX);
	verify(!Dnum(INT_MIN - 1LL).to_int(&n));
	verify(!Dnum(INT_MAX + 1LL).to_int(&n));
}

//-------------------------------------------------------------------

BENCHMARK(dnum_div) {
	Dnum x(12345678);
	Dnum y(87654321);
	while (nreps-- > 0)
		(void) (x / y);
}

BENCHMARK(dnum_div2) {
	Dnum x("1234567890123456");
	Dnum y("9876543210987654");
	while (nreps-- > 0)
		(void) (x / y);
}

BENCHMARK(dnum_mul) {
	Dnum x(12345678);
	Dnum y(87654321);
	while (nreps-- > 0)
		(void) (x * y);
}

BENCHMARK(dnum_mul2) {
	Dnum x("1234567890123456");
	Dnum y("9876543210987654");
	while (nreps-- > 0)
		(void) (x * y);
}

BENCHMARK(dnum_add) {
	Dnum x("123456.78");
	Dnum y("876543.21");
	while (nreps-- > 0)
		(void) (x + y);
}

BENCHMARK(dnum_add2) {
	Dnum x("1234567890123456");
	Dnum y("987654321.0987654");
	while (nreps-- > 0)
		(void) (x + y);
}

BENCHMARK(dnum_pack) {
	Dnum x(12345678);
	char buf[20];
	while (nreps-- > 0) {
		x.pack(buf);
		prev_coef = 0; // clear cache
	}
}

BENCHMARK(dnum_pack2) {
	Dnum x("9876543210987654");
	char buf[20];
	while (nreps-- > 0) {
		x.pack(buf);
		prev_coef = 0; // clear cache
	}
}
BENCHMARK(dnum_unpack) {
	Dnum x(12345678);
	char buf[20];
	int n = x.packsize();
	x.pack(buf);
	while (nreps-- > 0)
		(void) Dnum::unpack(gcstring::noalloc(buf, n));
}

BENCHMARK(dnum_unpack2) {
	Dnum x("9876543210987654");
	char buf[20];
	int n = x.packsize();
	x.pack(buf);
	while (nreps-- > 0)
		(void) Dnum::unpack(gcstring::noalloc(buf, n));
}

// ------------------------------------------------------------------

#define CK(x, y) \
	if ((x) != (y)) \
	return OSTR("got " << (x))

PORTTEST(dnum_add) {
	verify(args.size() == 3);
	Dnum x(args[0].str());
	Dnum y(args[1].str());
	Dnum z(args[2].str());
	CK(x + y, z);
	CK(y + x, z);
	return nullptr;
}

PORTTEST(dnum_sub) {
	verify(args.size() == 3);
	Dnum x(args[0].str());
	Dnum y(args[1].str());
	Dnum z(args[2].str());
	CK(x - y, z);
	if (!z.isZero())
		CK(y - x, -z);
	return nullptr;
}

PORTTEST(dnum_mul) {
	verify(args.size() == 3);
	Dnum x(args[0].str());
	Dnum y(args[1].str());
	Dnum z(args[2].str());
	CK(x * y, z);
	CK(y * x, z);
	return nullptr;
}

PORTTEST(dnum_div) {
	verify(args.size() == 3);
	Dnum x(args[0].str());
	Dnum y(args[1].str());
	Dnum z(args[2].str());
	CK(x / y, z);
	return nullptr;
}

PORTTEST(dnum_cmp) {
	int n = args.size();
	for (int i = 0; i < n; ++i) {
		Dnum x(args[i].str());
		if (Dnum::cmp(x, x) != 0)
			return OSTR(x << " not equal to itself");
		for (int j = i + 1; j < n; ++j) {
			Dnum y(args[j].str());
			if (Dnum::cmp(x, y) != -1 || Dnum::cmp(y, x) != +1)
				return OSTR(x << " not less than " << y);
		}
	}
	return nullptr;
}
