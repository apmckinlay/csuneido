#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"
#include "named.h"

const char DOT = 1;
const char DYN = 2;
const char PUB = 4;

// abstract base class for BuiltinFuncs and SuFunction
class Func : public SuValue
	{
public:
	NAMED

	Value call(Value self, Value member, 
		short nargs, short nargnames, short* argnames, int each) override;

	short nparams = 0;
	bool rest = false;
	short* locals = nullptr;
	short ndefaults = 0;
	Value* literals = nullptr;
	char* flags = nullptr; // for dot and dyn params
	bool isMethod = false;

	void out(Ostream& out) const override;
	void args(short nargs, short nargnames, short* argnames, int each);

private:
	Value params();
	};

// abstract base class for built-in functions
class BuiltinFuncs : public Func
	{
	const char* type() const override
		{ return "BuiltinFunction"; }
	};

typedef Value (*BuiltinFn)();

// built-in functions
class BuiltinFunc : public BuiltinFuncs
	{
public:
	BuiltinFunc(const char* name, const char* params, BuiltinFn f);
	Value call(Value self, Value member, 
		short nargs, short nargnames, short* argnames, int each) override;
	void out(Ostream& out) const override;
private:
	Value (*pfn)();
	};

// expand arguments onto stack for fn(@args)
void argseach(short& nargs, short& nargnames, short*& argnames, int& each);
