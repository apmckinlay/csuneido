#pragma once
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

#include <limits.h>
#include <string.h>
#include "std.h"
#include "suvalue.h"

class Ostream;

enum Sign { MINUS = 0, PLUS = 1 };

const int NDIGITS = 4;	// 16 decimal digits
const int MAXDIGITS = 16;
class random_class { };
extern random_class randnum;

// decimal representation, floating point number values
class SuNumber : public SuValue
	{
private:
	char sign;
	schar exp;	// exponent
	short digits[NDIGITS];	// most significant digit first
public:
	void* operator new(size_t n);
	void* operator new(size_t n, void* p)
		{ return p; }
	friend Ostream& operator<<(Ostream&, const SuNumber&);

	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;

	int integer() const override;
	int64 bigint() const;
	gcstring to_gcstr() const override;
	void out(Ostream& os) const override;
	size_t hashfn() const override;
	int symnum() const override;
	bool int_if_num(int* pn) const override;

	SuNumber* number() override
		{ return this; }

	int order() const override;
	bool lt(const SuValue& x) const override;
	bool eq(const SuValue& x) const override;

	size_t packsize() const override;
	void pack(char* buf) const override;
	static SuNumber* unpack(const gcstring& s);

	static Value literal(const char* s);

	explicit SuNumber(long);
	static SuNumber* from_int64(int64);
	explicit SuNumber(const char* buf);
	explicit SuNumber(const SuNumber* x)
		{ *this = *x; }

	char* format(char* buf) const;

	char* mask(char* buf, const char* mask) const;

	double to_double() const;
	static SuNumber* from_float(float x);
	static SuNumber* from_double(double x);

	friend int cmp(const SuNumber*, const SuNumber*);
	friend bool close(const SuNumber*, const SuNumber*);
	friend SuNumber* add(const SuNumber*, const SuNumber*);
	friend SuNumber* sub(const SuNumber*, const SuNumber*);
	friend SuNumber* mul(const SuNumber*, const SuNumber*);
	friend SuNumber* div(const SuNumber*, const SuNumber*);
	friend SuNumber* neg(const SuNumber* x);

	static SuNumber zero, one, minus_one, infinity, minus_infinity;
private:
	SuNumber(char s, schar e);
	SuNumber(char s, schar e, const short* d);
	explicit SuNumber(random_class);
	friend class test_number;
	SuNumber& toint();
	SuNumber& tofrac();
	void check() const;
	bool is_zero() const
		{ return exp == SCHAR_MIN; }
	bool is_infinity() const	// plus or minus infinity
		{ return exp == SCHAR_MAX; }
	SuNumber* negate()
		{
		if (! is_zero())
			sign = 1 - sign;
		return this;
		}
	void shift_right();
	void shift_left();
	friend int ucmp(const SuNumber*, const SuNumber*);
	friend SuNumber* uadd(const SuNumber*, const SuNumber*);
	friend SuNumber* usub(const SuNumber*, const SuNumber*, SuNumber*);
	friend SuNumber* round(SuNumber*, int digits, char mode);
	long first9() const
		{ return digits[0] * 100000L + digits[1] * 10L + digits[2] / 1000; }
	long first8() const
		{ return digits[0] * 10000L + digits[1]; }
	long first5() const
		{ return digits[0] * 10L + digits[1] / 1000; }
	void operator -=(const SuNumber& y)
		{
		SuNumber tmp(this);
		(void) usub(&tmp, &y, this);
		}
	void product(const SuNumber&, short);
	};

inline SuNumber* neg(const SuNumber* x)
	{ return (new SuNumber(x))->negate(); }

inline bool operator==(const SuNumber& x, const SuNumber& y)
	{ return cmp(&x, &y) == 0; }

inline bool operator<(const SuNumber& x, const SuNumber& y)
	{ return cmp(&x, &y) < 0; }

inline SuNumber& operator+(const SuNumber& x, const SuNumber& y)
	{ return *add(&x, &y); }

inline SuNumber& operator-(const SuNumber& x, const SuNumber& y)
	{ return *sub(&x, &y); }

inline SuNumber& operator*(const SuNumber& x, const SuNumber& y)
	{ return *mul(&x, &y); }

inline SuNumber& operator/(const SuNumber& x, const SuNumber& y)
	{ return *div(&x, &y); }

int numlen(const char* s);

SuNumber* neg(Value x);
