#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "type.h"
#include "func.h"

// allows Suneido user code to call dll functions
class Dll : public BuiltinFuncs
	{
public:
	Dll(short rtype, char* library, char* name, TypeItem* p, short* ns, short n);
	Value call(Value self, Value member, 
		short nargs, short nargnames, short* argnames, int each) override;
	void out(Ostream&) const override;
	const char* type() const override
		{ return "Dll"; }
private:
	short dll;
	short fn;
	TypeParams params;
	short rtype;
	void* pfn;
	bool trace;
	};
