#ifndef CALLBACK_H
#define CALLBACK_H

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
#include "named.h"

// a Type for defining calls from external code
class Callback : public TypeMulti
	{
public:
	NAMED
	Callback(TypeItem* it, ushort* ns, short n)	: TypeMulti(it, n)
		{ mems = dup(ns, n); }
	int size()
		{ return sizeof (void*); }
	void put(char*& dst, char*& dst2, const char* lim2, Value x);
	Value get(char*& src, Value x)
		{ src += sizeof (void*); return x; }
	void out(Ostream& os);
	long callback(Value, char*);
private:
	ushort* mems;
	};

#endif

