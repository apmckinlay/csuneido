#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "type.h"
#include "named.h"

// user defined struct Type for dll interface
// similar to TypeParams
class Structure : public TypeMulti {
public:
	NAMED
	Structure(TypeItem* it, short* ms, int n);
	void put(char*& dst, char*& dst2, const char* lim2, Value x) override;
	Value get(const char*& src, Value x) override;
	void getbyref(const char*& src, Value x) override {
		get(src, x);
	}
	void out(Ostream& os) const override;
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;

private:
	short* mems;
};
