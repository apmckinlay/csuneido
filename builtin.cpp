// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "builtin.h"
#include "fatal.h"
#include "gsl-lite.h"
#include "func.h"

const int MAX_BUILTINS = 100;
int nbuiltins = 0;
Builtin* builtins[MAX_BUILTINS];

Builtin::Builtin(BuiltinFn f, const char* n, const char* p) 
	: fn(f), name(n), params(p)
	{
	if (nbuiltins >= MAX_BUILTINS)
		fatal("too many BUILTIN functions - increase MAX_BUILTINS");
	builtins[nbuiltins++] = this;
	}

void builtin(int gnum, Value value); // in library.cpp

void install_builtin_functions()
	{
	for (auto b : gsl::span<Builtin*>(builtins, nbuiltins))
		{
		auto p = new BuiltinFunc(b->name, b->params, b->fn);
		builtin(p->named.num, p);
		}
	}
