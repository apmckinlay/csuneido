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

#include "sumethod.h"
#include "interp.h"
#include "symbols.h"
#include "sufunction.h"

void SuMethod::out(Ostream& os)
	{
	//os << "OBJECT: " << object << endl;
	//os << "METHOD: " << method << endl;
	//os << "SUFN: " << sufn->named.info() << endl;
	if (sufn)
		{
		if (sufn->named.lib != "" || sufn->named.parent)
			os << sufn;
		else
			os << "/* method " << sufn->named.name() << " */";
		}
	else
		os << "/* method */";
//	os << method;
	}

bool SuMethod::eq(const SuValue& y) const
	{
	if (const SuMethod* m = dynamic_cast<const SuMethod*>(&y))
		return (void*) object == (void*) m->object && method == m->method; 
	else
		return false;
	}

size_t SuMethod::hashfn()
	{
	return (size_t) (void*) object + method.hash();
	}

Value SuMethod::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	if (member == CALL)
		return object.call(object, method, nargs, nargnames, argnames, each);
	else
		return sufn->call(sufn, member, nargs, nargnames, argnames, each);
	}
