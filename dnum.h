#pragma once
// Copyright (c) 2018 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <cstdint>

class Ostream;
class gcstring;

#pragma pack(4)
class Dnum
	{
public:
	constexpr Dnum() : coef(0), sign(0), exp(0)
		{}
	Dnum(const Dnum&) = default;
	explicit Dnum(int n);
	explicit Dnum(int64_t n);
	explicit Dnum(const char* s);
	// used for results of operations, normalizes to max coefficient
	// public for SuDnum unpack
	Dnum(int s, uint64_t c, int e);


	friend bool operator==(const Dnum& x, const Dnum& y);
	friend Ostream& operator<<(Ostream& os, const Dnum& dn);
	char* tostr(char* dst, int len) const;
	int getdigits(char* buf) const;
	gcstring show() const;
	gcstring to_gcstr() const;
	bool isZero() const
		{ return sign == 0; }
	bool isInf() const;
	int signum() const
		{ return sign < 0 ? -1 : sign > 0 ? +1 : 0; }
	int8_t getexp() const
		{ return exp; }
	int64_t getcoef() const
		{ return coef; }
	size_t hashfn() const;

	Dnum operator-() const;
	Dnum abs() const;
	static int cmp(const Dnum& x, const Dnum& y);
	friend bool operator<(const Dnum& x, const Dnum& y)
		{ return cmp(x, y) < 0; }
	friend Dnum operator*(const Dnum& x, const Dnum& y);
	friend Dnum operator/(const Dnum& x, const Dnum& y);
	friend Dnum operator+(const Dnum& x, const Dnum& y);
	friend Dnum operator-(const Dnum& x, const Dnum& y)
		{ return x + -y; }
	size_t packsize() const;
	void pack(char* buf) const;
	static Dnum unpack(const gcstring& s);

	double to_double() const;
	static Dnum from_double(double d);

	enum class RoundingMode { UP, DOWN, HALF_UP };
	Dnum round(int r, RoundingMode mode = RoundingMode::HALF_UP) const;
	Dnum integer(RoundingMode mode = RoundingMode::DOWN) const;
	Dnum frac() const;

	int32_t to_int32() const;
	int64_t to_int64() const;
	int intOrMin() const;
	
	static Dnum inf(int sign = +1);

	static const int MAX_DIGITS = 16;
	static const Dnum ZERO;
	static const Dnum ONE;
	static const Dnum MINUS_ONE;
	static const Dnum INF;
	static const Dnum MINUS_INF;
	static const Dnum MAX_INT;
	enum { STRLEN = 24 };

private:
	friend bool align(Dnum& x, Dnum& y);
	friend Dnum uadd(const Dnum& x, const Dnum& y);
	friend Dnum usub(const Dnum& x, const Dnum& y);
	friend static void test_dnum_misc();
	friend static bool almostSame(const Dnum& x, const Dnum& y);

	uint64_t coef;
	int8_t sign;
	int8_t exp;
	};
#pragma pack()
