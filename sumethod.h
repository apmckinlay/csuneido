#ifndef SUMETHOD_H
#define SUMETHOD_H

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

#include "value.h"
#include "suvalue.h"

class SuFunction;

// a value that points to a method of a particular instance of an object
class SuMethod : public SuValue
	{
public:
	explicit SuMethod(Value o, Value meth, SuFunction* f)
		: object(o), method(meth), sufn(f)
		{ }
	void out(Ostream& os);
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
	bool eq(const SuValue& x) const;
	size_t hashfn();

	SuFunction* fn()
		{ return sufn; }
private:
	Value object;
	Value method;
	SuFunction* sufn;
	};

#endif
