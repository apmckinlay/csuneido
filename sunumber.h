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

#include "suvalue.h"
#include "dnum.h"
#include "gcstring.h"

class SuNumber : public SuValue
	{
public:
	SuNumber(Dnum d) : dn(d)
		{}
	explicit SuNumber(long n) : dn(n)
		{}
	explicit SuNumber(const char* buf) : dn(buf)
		{}

	static SuNumber* from_int64(int64_t n)
		{ return new SuNumber(Dnum(n)); }

	// handles 0x...
	static Value literal(const char* s);

	void out(Ostream&) const override;
	void* operator new(size_t n);  // NOLINT
	void* operator new(size_t n, void* p)
		{ return p; }

	int order() const override;
	bool lt(const SuValue& y) const override;
	bool eq(const SuValue& y) const override;

	friend int cmp(const SuNumber* x, const SuNumber* y)
		{ return Dnum::cmp(x->dn, y->dn); }
	friend SuNumber* add(const SuNumber* x, const SuNumber* y)
		{ return new SuNumber(x->dn + y->dn); }
	friend SuNumber* sub(const SuNumber* x, const SuNumber* y)
		{ return new SuNumber(x->dn - y->dn); }
	friend SuNumber* mul(const SuNumber* x, const SuNumber* y)
		{ return new SuNumber(x->dn * y->dn); }
	friend SuNumber* div(const SuNumber* x, const SuNumber* y)
		{ return new SuNumber(x->dn / y->dn); }
	friend SuNumber* neg(const SuNumber* x)
		{ return new SuNumber(-x->dn); }

	Value call(Value self, Value member,
		short nargs, short nargnames, uint16_t* argnames, int each) override;

	// buf must be larger than mask
	char* format(char* buf, const char* mask) const;

	SuNumber* number() override
		{ return this; }

	int integer() const override;
	int64_t bigint() const
		{ return dn.to_int64(); }
	int trunc() const;
	gcstring to_gcstr() const override
		{ return dn.to_gcstr(); }
	size_t hashfn() const override;
	int symnum() const override;
	bool int_if_num(int* pn) const override;
	double to_double() const
		{ return dn.to_double(); }
	static SuNumber* from_double(double x)
		{ return new SuNumber(Dnum::from_double(x)); }
	static SuNumber* from_float(float x)
		{ return new SuNumber(Dnum::from_double(x)); }
	SuNumber* round(int digits, Dnum::RoundingMode mode) const
		{ return new SuNumber(dn.round(digits, mode)); }

	size_t packsize() const override;
	void pack(char* buf) const override;
	static Value unpack(const gcstring& buf);

	static SuNumber zero, one, minus_one, infinity, minus_infinity;

private:
	Dnum dn;
	};

int numlen(const char* s); // in numlen.cpp
