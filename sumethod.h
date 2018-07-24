#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"
#include "suvalue.h"

class SuFunction;

// a value that points to a method of a particular instance of an object
class SuMethod : public SuValue {
public:
	explicit SuMethod(Value o, Value meth, SuFunction* f)
		: object(o), method(meth), sufn(f) {
	}
	void out(Ostream& os) const override;
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
	bool eq(const SuValue& x) const override;
	size_t hashfn() const override;
	const Named* get_named() const override;

	SuFunction* fn() const {
		return sufn;
	}

private:
	Value object;
	Value method;
	SuFunction* sufn;
};
