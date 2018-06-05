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

#define TOVAL(i) (str[i] ? new SuString(args[i]) : compile(args[i]))

PORTTEST(method)
	{
	Value ob = TOVAL(0);
	auto method = args[1];
	Value expected = TOVAL(args.size() - 1);
	KEEPSP
	for (int i = 2; i < args.size() - 1; ++i)
		PUSH(TOVAL(i));
	Value result = ob.call(ob, method, args.size() - 3);
	return result == expected ? nullptr : OSTR("got: " << result);
	}
