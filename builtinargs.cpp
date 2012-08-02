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
for built-in functions/methods that require more than PRIM
Used by BuiltinClass
- does argseach
- handles getting arguments from named or unnamed
- handles default argument values
- must get arguments in order
- can handle variable args with getNext and curName
e.g.
	BuiltinArgs(nargs, nargnames, argnames, each);
	args.usage("usage: ...");
	char* first = args.getstr("first");
	Value second = args.getValue("second", SuFalse);
	args.end();
*/

// TODO don't do argseach
// if each then read args object instead of stack
// maybe have a builtinArgs function that returns one of two kinds of BuiltinArgs
// either current stack one, or new args object one

#include "builtinargs.h"
#include "func.h"
#include "interp.h"
#include "except.h"

BuiltinArgs::BuiltinArgs(short& nargs_, short& nargnames_, ushort*& argnames_, int& each)
	: msg1("invalid arguments"), msg2(""), msg3(""), i(0), def(false)
	{
	argseach(nargs_, nargnames_, argnames_, each);
	nargs = nargs_;
	nargnames = nargnames_;
	argnames = argnames_;
	unnamed = nargs - nargnames;
	}

Value BuiltinArgs::getval(char* name)
	{
	int arg;
	if (i < unnamed)
		arg = i++;
	else
		{
		const int sym = symnum(name);
		int j = 0;
		for (; j < nargnames; ++j)
			if (argnames[j] == sym)
				break ;
		if (j >= nargnames)
			return Value();
		else
			taken.push(sym);
		arg = unnamed + j;
		}
	return ARG(arg);
	}

Value BuiltinArgs::getValue(char* name)
	{
	Value val = getval(name);
	if (! val)
		exceptUsage();
	return val;
	}

void BuiltinArgs::ckndef()
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
	while (i < nargs && i > unnamed && taken.member(argnames[i - unnamed]))
		++i;
	return i < nargs ? ARG(i++) : Value();
	}

ushort BuiltinArgs::curName()
	{
	int cur = i - 1;
	return cur < unnamed ? 0 :  argnames[cur - unnamed];
	}

Value BuiltinArgs::getNextUnnamed()
	{
	return i < unnamed ? ARG(i++) : Value();
	}

void BuiltinArgs::exceptUsage()
	{
	except(msg1 << msg2 << msg3);
	}
