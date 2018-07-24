#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "suvalue.h"

// date and time values
class SuDate : public SuValue {
public:
	SuDate();
	SuDate(int d, int t);

	void* operator new(size_t n);
	void* operator new(size_t n, void* p) {
		return p;
	}

	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;

	void out(Ostream& out) const override;

	// database packing
	size_t packsize() const override {
		return 1 + sizeof(int) + sizeof(int);
	}
	void pack(char* buf) const override;
	static SuDate* unpack(const gcstring& s);
	static Value literal(const char* s);

	// for use as subscript
	size_t hashfn() const override {
		return date ^ time;
	}

	int order() const override;
	bool lt(const SuValue& x) const override;
	bool eq(const SuValue& x) const override;

	bool operator<(const SuDate& d) const;

	SuDate& increment();
	static SuDate* timestamp();
	static Value parse(const char* s, const char* order = "yMd");
	static int64_t minus_ms(SuDate* d1, SuDate* d2);
	static Value instantiate(
		short nargs, short nargnames, short* argnames, int each);

private:
	using Mfn = Value (SuDate::*)(short, short, short*, int);
	static Mfn method(Value member);
	Value FormatEn(short nargs, short nargnames, short* argnames, int each);
	Value Plus(short nargs, short nargnames, short* argnames, int each);
	Value MinusDays(short nargs, short nargnames, short* argnames, int each);
	Value MinusSeconds(short nargs, short nargnames, short* argnames, int each);
	Value Year(short nargs, short nargnames, short* argnames, int each);
	Value Month(short nargs, short nargnames, short* argnames, int each);
	Value Day(short nargs, short nargnames, short* argnames, int each);
	Value Hour(short nargs, short nargnames, short* argnames, int each);
	Value Minute(short nargs, short nargnames, short* argnames, int each);
	Value Second(short nargs, short nargnames, short* argnames, int each);
	Value Millisecond(short nargs, short nargnames, short* argnames, int each);
	Value WeekDay(short nargs, short nargnames, short* argnames, int each);

	int date;
	int time;
};

class SuDateClass : public SuValue {
public:
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
	void out(Ostream& os) const override;
	const char* type() const override;
};
