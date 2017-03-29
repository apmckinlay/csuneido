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

#include "structure.h"
#include "interp.h"
#include "sustring.h"
#include "suobject.h"
#include "symbols.h"

Structure::Structure(TypeItem* it, ushort* ms, int n) : TypeMulti(it, n)
	{
	mems = dup(ms, n);;
	}

void Structure::put(char*& dst, char*& dst2, const char* lim2, Value x)
	{
	if (! x)
		x = new SuObject();
	SuObject* ob = x.object();
	for (int i = 0; i < nitems; ++i)
		items[i].type().put(dst, dst2, lim2, ob->get(symbol(mems[i])));
	}

Value Structure::get(char*& src, Value x)
	{
	if (! x)
		x = new SuObject;//(nitems);
	SuObject* ob = x.object();
	for (int i = 0; i < nitems; ++i)
		{
		Value old = ob->get(symbol(mems[i]));
		Value now = items[i].type().get(src, old);
		if (! old || old != now)
			ob->put(symbol(mems[i]), now);
		}
	return x;
	}

void Structure::out(Ostream& os) const
	{
	if (named.num)
		os << named.name() << " /* " << named.lib << " struct */";
	else
		{
		os << "struct { ";
		for (int i = 0; i < nitems; ++i)
			{
			items[i].out(os);
			os << " " << symstr(mems[i]) << "; ";
			}
		os << '}';
		}
	}

Value Structure::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value SIZE("Size");
	static Value MODIFY("Modify");

	if (member == SIZE)
		{
		if (nargs != 0)
			except("usage: struct.Size()");
		return size();
		}
	else if (member == CALL)
		{
		if (nargs != 1)
			except("usage: struct(value)");
		int n;
		Value arg = ARG(0);
		if (arg.ob_if_ob())
			{
			// convert object to structure in string
			n = size();
			if (n > 512)
				except("structure too big");
			char buf[1024];
			char* dst = buf;
			char* dst2 = buf + n;
			char* lim2 = buf + 1024;
			put(dst, dst2, lim2, arg);
			verify(dst == buf + n);
			return new SuString(buf, dst2 - buf);
			}
		else if (arg.int_if_num(&n))
			{
			// interpret number as pointer to structure
			if (! n)
				return SuFalse;
			// TODO: use VirtualQuery to check if valid
			char* s = (char*) n;
			return get(s, Value());
			}
		else
			{
			// interpret string as structure
			gcstring s = arg.gcstr();

			// TODO: should check whether s is big enough for pointed to data
			if (s.size() < size())
				except("string not long enough to convert to this structure");
			// TODO: get should make sure pointed to data is within s
			char* src = s.buf();
			return get(src, Value());
			}
		}
	else if (member == MODIFY)
		{
		if (nargs != 3)
			except("usage: struct.Modify(address, member, value)");
		char* dst = (char*) ARG(0).integer();
		ushort mem = ARG(1).symnum();
		int i;
		for (i = 0; i < nitems; ++i)
			{
			if (mems[i] == mem)
				{
				char* dst2 = nullptr;
				items[i].type().put(dst, dst2, 0, ARG(2));
				break ;
				}
			else
				dst +=  items[i].type().size();
			}
		if (i >= nitems)
			except("member not found: " << ARG(1));
		}
	else
		method_not_found("struct", member);
	return Value();
	}
