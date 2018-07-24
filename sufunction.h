#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "func.h"

// debug information for compiled SuFunction's
struct Debug {
	Debug(int s, int c) : si(s), ci(c) {
	}

	int si;
	int ci;
};

// user defined, interpreted functions
class SuFunction : public Func {
public:
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;

	void source(Ostream&, int ci);
	int source(int ci, int* pn);
	void disasm(Ostream&);
	int disasm1(Ostream&, int);

	uint8_t* code = nullptr;
	int nc = 0;
	Debug* db = nullptr;
	short nd = 0;
	short nlocals = 0;
	short nliterals = 0;
	const char* src = "";
	const char* className = nullptr; // for dot params, used by SuFunction call

private:
	void dotParams(Value self);
	const char* mem(int& ci);
};
