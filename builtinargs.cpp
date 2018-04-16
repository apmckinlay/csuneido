/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2003 Suneido Software Corp.
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

/*
This is the current preferred method of argument handling
for built-in functions/methods that require more than BUILTIN
Used by BuiltinClass
- does argseach
- handles getting arguments from named or unnamed
- handles default argument values
- must get arguments in order
- can handle variable args with getNext and curName
e.g.
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("...");
	char* first = args.getstr("first");
	Value second = args.getValue("second", SuFalse);
	args.end();
for no arguments you can do:
	args.usage("...").end();
*/

// TODO don't do argseach
// if each then read args object instead of stack
// maybe have a builtinArgs function that returns one of two kinds of BuiltinArgs
// either current stack one, or new args object one

// TODO cache pointer to args

#include "builtinargs.h"
#include "func.h"
#include "interp.h"
#include "except.h"

BuiltinArgs::BuiltinArgs(short& nargs_,
	short& nargnames_, ushort*& argnames_, int& each)
	{
	if (nargs_ != 0)
		argseach(nargs_, nargnames_, argnames_, each);
	nargs = nargs_;
	nargnames = nargnames_;
	argnames = argnames_;
	unnamed = nargs - nargnames;
	}

// takes the next unnamed
// or if no unnamed then looks for a matching named
Value BuiltinArgs::getval(const char* name)
	{
	if (i < unnamed)
		return ARG(i++);
	else
		return getNamed(name);
	}

Value BuiltinArgs::getNamed(const char* name)
	{
	const int sym = symnum(name);
	int j = 0;
	for (; j < nargnames; ++j)
		if (argnames[j] == sym)
			break;
	if (j >= nargnames)
		return Value();
	taken.add(sym);
	return ARG(unnamed + j);
	}

Value BuiltinArgs::getValue(const char* name)
	{
	Value val = getval(name);
	if (! val)
		exceptUsage();
	return val;
	}

void BuiltinArgs::ckndef() const
	{
	verify(! def);
	}

void BuiltinArgs::end()
	{
	if (i < unnamed)
		exceptUsage();
	}

Value BuiltinArgs::getNext()
	{
	while (i < nargs && i >= unnamed && taken.has(argnames[i - unnamed]))
		++i;
	return i < nargs ? ARG(i++) : Value();
	}

ushort BuiltinArgs::curName() const
	{
	int cur = i - 1;
	return cur < unnamed ? 0 :  argnames[cur - unnamed];
	}

Value BuiltinArgs::getNextUnnamed()
	{
	return i < unnamed ? ARG(i++) : Value();
	}

void BuiltinArgs::exceptUsage() const
	{
	except("usage: " << msg1 << msg2 << msg3);
	}

Value BuiltinArgs::call(Value fn, Value self, Value method)
	{
	verify(i <= unnamed);
	verify(taken.empty());
	return fn.call(self, method, nargs - i, nargnames, argnames, -1);
	}
