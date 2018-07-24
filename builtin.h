#pragma once
// Copyright (c) 2004 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"

typedef Value (*BuiltinFn)();

struct Builtin {
	Builtin(BuiltinFn fn, const char* name, const char* decl);

	BuiltinFn fn;
	const char* name;
	const char* params;
};

#define BUILTIN(name, decl) \
	static Value su_##name(); \
	static Builtin builtin_##name(su_##name, #name, decl); \
	static Value su_##name()

void install_builtin_functions();
