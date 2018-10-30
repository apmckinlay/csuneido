#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "membase.h"

class SuObject;
class Ostream;

// user defined classes
class SuClass : public MemBase {
public:
	NAMED
	SuClass() : named("."), base(0) {
	}
	explicit SuClass(short b) : named("."), base(b) {
	}
	void out(Ostream&) const override;
	void show(Ostream& out) const override;
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
	Value getdata(Value) override;

	bool hasMethod(Value mem) {
		return !!method_class(mem);
	}

protected:
	virtual Value get(Value m) const; // overridden by SuSocketServer
private:
	void put(Value m, Value x); // used by compile
	Value get2(Value self, Value member);
	Value get3(Value member);
	Value mbclass() override {
		return this;
	}
	Value parent() override;
	bool readonly() override {
		return true;
	}

	const short base;
	bool has_getter = true;

	friend class SuInstance;
	friend struct ClassContainer;
};
