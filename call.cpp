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

#include "call.h"
#include "interp.h"
#include "globals.h"
#include "sustring.h"
#include "ostreamstr.h"
#include "exceptimp.h"

// used by QueryParser for extend macros
// used by Query expressions FunCall

Value call(const char* fname, Lisp<Value> args)
	{
	KEEPSP
	int nargs = 0;
	for (; ! nil(args); ++args, ++nargs)
		PUSH(*args);
	return docall(globals[fname], CALL, nargs, 0, 0, -1);
	}

// used by sunapp
char* trycall(const char* fn, char* arg, int* plen)
	{
	char* str;
	try
		{
		gcstring gcstr;
		Value result = call(fn, lisp(Value(new SuString(arg))));
		if (result == SuFalse)
			return 0;
		gcstr = result.gcstr();
		if (plen)
			*plen = gcstr.size();
		str = gcstr.str();
		}
	catch (const Except& e)
		{
		OstreamStr oss;
		oss << fn << "(" << arg << ") => " << e;
		str = oss.str();
		if (plen)
			*plen = oss.size();
		}
	return str;
	}
