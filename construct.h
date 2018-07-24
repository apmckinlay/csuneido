#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "func.h"

// shortcut function to create an instance of a class
class Construct : public Func {
public:
	Construct();
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
};
