#ifndef SUCLASS_H
#define SUCLASS_H

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

#include "suobject.h"
#include "named.h"

class Ostream;

typedef Value (*Factory)(SuValue*);

// user defined classes
class SuClass : public SuObject
	{
public:
	NAMED
	explicit SuClass(short b) :
		named("."), base(b), factory(object_factory)
		{ }
	explicit SuClass(const SuClass& c) :
		SuObject(c), named("."), base(c.base), factory(object_factory)
		{ }
	explicit SuClass(Factory f) : factory(f)
		{ }
	void out(Ostream&);
	Value call(Value self, Value member, short nargs, short nargnames, ushort *argnames, int each);
	Value getdata(Value);

	static Value object_factory(SuValue* suclass)
		{
		SuObject* ob = new SuObject;
		ob->myclass = suclass;
		return ob;
		}

	virtual size_t packsize() const;
	virtual void pack(char* buf) const;
	static SuClass* unpack(const gcstring& s);

	short base;
	Factory factory;
	};

// conceptually RootClass is a kind of Class
// but practically, it doesn't need to derive from SuClass
// since it never has any members
class RootClass : public SuValue
	{
public :
	void out(Ostream& os)
		{ os << "Object"; }
	Value call(Value self, Value member, short nargs, short nargnames, ushort *argnames, int each);
	Value getdata(Value)
		{ return Value(); }
	static Value notfound(Value self, Value member, short nargs, short nargnames, ushort *argnames, int each);
	};

#endif
