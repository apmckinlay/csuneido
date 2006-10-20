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

#include "builtinargs.h"
#include "func.h"
#include "interp.h"
#include "except.h"

BuiltinArgs::BuiltinArgs(short n, short nn, ushort *an, int each)
	: nargs(n), nargnames(nn), argnames(an),
	unnamed(nargs - nargnames), i(0), def(false)
	{
	argseach(nargs, nargnames, argnames, each);
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
		arg = unnamed + j;
		}
	return ARG(arg);
	}

Value BuiltinArgs::getValue(char* name)
	{
	Value val = getval(name);
	if (! val)
		except(msg);
	return val;
	}

void BuiltinArgs::ckndef()
	{
	verify(! def);
	}

void BuiltinArgs::end()
	{
	if (i < unnamed)
		except(msg);
	}
