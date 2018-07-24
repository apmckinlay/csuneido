#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "suvalue.h"

class gcstring;

// a boolean value (true or false)
class SuBoolean : public SuValue {
public:
	int integer() const override;
	void out(Ostream& os) const override;
	gcstring to_gcstr() const override;
	SuNumber* number() override; // defined by SuNumber & SuString

	size_t packsize() const override;
	void pack(char* buf) const override;
	static SuBoolean* unpack(const gcstring& s);

	static SuBoolean* literal(const char* s);

	int order() const override;
	bool lt(const SuValue& x) const override;
	bool eq(const SuValue& x) const override;

	static SuBoolean* t;
	static SuBoolean* f;

private:
	// private constructor to ensure only two instances
	explicit SuBoolean(int x) : val(x ? 1 : 0) {
	}

	bool val;
};
