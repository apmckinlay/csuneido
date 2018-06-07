/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2018 Suneido Software Corp.
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

#include "porttest.h"
#include "value.h"
#include "sustring.h"
#include "compile.h"
#include "interp.h"
#include "ostreamstr.h"
#include "exceptimp.h"

#define TOVAL(i) (str[i] ? new SuString(args[i]) : compile(args[i].str()))

PORTTEST(method)
	{
	Value ob = TOVAL(0);
	auto method = new SuString(args[1]);
	Value expected = TOVAL(args.size() - 1);
	KEEPSP
	for (int i = 2; i < args.size() - 1; ++i)
		PUSH(TOVAL(i));
	Value result = ob.call(ob, method, args.size() - 3);
	return result == expected ? nullptr : OSTR("got: " << result);
	}

PORTTEST(execute)
	{
	gcstring expected = "**notfalse**";
	if (args.size() > 1)
		expected = args[1];
	Value result;
	bool ok;
	if (expected == "throws")
		{
		try
			{
			result = run(args[0].str());
			ok = false;
			}
		catch (Except& e)
			{
			result = new SuString(e.gcstr());
			ok = e.gcstr().has(args[2]);
			}
		}
	else if (expected == "**notfalse**")
		{
		result = run(args[0].str());
		ok = result != SuFalse;
		}
	else
		{
		result = run(args[0].str());
		Value expectedVal = compile(expected.str());
		ok = result == expectedVal;
		}
	if (!ok)
		return OSTR("got: " << result);
	return nullptr;
	}
