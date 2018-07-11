#pragma once
// Copyright (c) 2017 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"
#include "membase.h"

class SuClass;

class SuInstance : public MemBase
	{
public:
	SuInstance(Value c) : myclass(c)
		{}

	Value getdata(Value) override;
	void putdata(Value, Value) override;
	Value call(Value self, Value member, short nargs,
		short nargnames, short* argnames, int each) override;

	bool eq(const SuValue& x) const override;
	size_t hashfn() const override;
	size_t hashcontrib() const override;

	void out(Ostream&) const override;
	gcstring to_gcstr() const override;

private:
	Value Copy(short nargs, short nargnames, short* argnames, int each);
	Value Delete(short nargs, short nargnames, short* argnames, int each);
	const char* toString() const;

	Value mbclass() override
		{ return myclass; }
	Value parent() override
		{ return myclass; }
	bool readonly() override
		{ return false; }

	// instance references its class directly, not by global
	// partly to allow instances of anonymous classes
	const Value myclass;
	};

