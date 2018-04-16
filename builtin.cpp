/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2000 Suneido Software Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation - version 2.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License in the file COPYING
 * for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

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
