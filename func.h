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

#include "value.h"
#include "named.h"

const char DOT = 1;
const char DYN = 2;
const char PUB = 4;

// abstract base class for BuiltinFunc and SuFunction
class Func : public SuValue
	{
public:
	NAMED

	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;

	short nparams = 0;
	bool rest = false;
	ushort* locals = nullptr;
	short ndefaults = 0;
	Value* literals = nullptr;
	char* flags = nullptr; // for dot and dyn params
	bool isMethod = false;

	void out(Ostream& out) const override;
	void args(short nargs, short nargnames, ushort* argnames, int each);

private:
	Value params();
	};

// abstract base class for Primitive and Dll
class BuiltinFunc : public Func
	{
	const char* type() const override
		{ return "BuiltinFunction"; }
	};

typedef Value (*PrimFn)();

// built-in functions
class Primitive : public BuiltinFunc
	{
public:
	explicit Primitive(PrimFn f, ...);
	Primitive(const char* decl, PrimFn f);
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;
	void out(Ostream& out) const override;
private:
	Value (*pfn)();
	};

// expand arguments onto stack for fn(@args)
void argseach(short& nargs, short& nargnames, ushort*& argnames, int& each);
