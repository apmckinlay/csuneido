#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "suvalue.h"
#include "dnum.h"
#include "gcstring.h"

class SuNumber : public SuValue {
public:
	SuNumber(Dnum d) : dn(d) {
	}
	explicit SuNumber(int n) : dn(n) {
	}
	explicit SuNumber(const char* buf) : dn(buf) {
	}

	static SuNumber* from_int64(int64_t n) {
		return new SuNumber(Dnum(n));
	}

	// handles 0x...
	static Value literal(const char* s);

	void out(Ostream&) const override;
	void* operator new(size_t n); // NOLINT
	void* operator new(size_t n, void* p) {
		return p;
	}

	int order() const override;
	bool lt(const SuValue& y) const override;
	bool eq(const SuValue& y) const override;

	friend int cmp(const SuNumber* x, const SuNumber* y) {
		return Dnum::cmp(x->dn, y->dn);
	}
	friend SuNumber* add(const SuNumber* x, const SuNumber* y) {
		return new SuNumber(x->dn + y->dn);
	}
	friend SuNumber* sub(const SuNumber* x, const SuNumber* y) {
		return new SuNumber(x->dn - y->dn);
	}
	friend SuNumber* mul(const SuNumber* x, const SuNumber* y) {
		return new SuNumber(x->dn * y->dn);
	}
	friend SuNumber* div(const SuNumber* x, const SuNumber* y) {
		return new SuNumber(x->dn / y->dn);
	}
	friend SuNumber* neg(const SuNumber* x) {
		return new SuNumber(-x->dn);
	}

	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;

	// buf must be larger than mask
	char* format(char* buf, const char* mask) const;

	SuNumber* number() override {
		return this;
	}

	int integer() const override;
	int64_t bigint() const {
		return dn.to_int64();
	}
	int trunc() const;
	gcstring to_gcstr() const override {
		return dn.to_gcstr();
	}
	size_t hashfn() const override;
	short symnum() const override;
	bool int_if_num(int* pn) const override;
	double to_double() const {
		return dn.to_double();
	}
	static SuNumber* from_double(double x) {
		return new SuNumber(Dnum::from_double(x));
	}
	static SuNumber* from_float(float x) {
		return new SuNumber(Dnum::from_double(x));
	}
	SuNumber* round(int digits, Dnum::RoundingMode mode) const {
		return new SuNumber(dn.round(digits, mode));
	}

	size_t packsize() const override;
	void pack(char* buf) const override;
	static Value unpack(const gcstring& buf);

	static SuNumber zero, one, minus_one, infinity, minus_infinity;

private:
	Dnum dn;
};

int numlen(const char* s); // in numlen.cpp
