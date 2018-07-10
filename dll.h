#pragma once
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

#include "type.h"
#include "func.h"

// allows Suneido user code to call dll functions
class Dll : public BuiltinFuncs
	{
public:
	Dll(short rtype, char* library, char* name, TypeItem* p, uint16_t* ns, short n);
	Value call(Value self, Value member, 
		short nargs, short nargnames, uint16_t* argnames, int each) override;
	void out(Ostream&) const override;
	const char* type() const override
		{ return "Dll"; }
private:
	short dll;
	short fn;
	TypeParams params;
	short rtype;
	void* pfn;
	bool trace;
	};
