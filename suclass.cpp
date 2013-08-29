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

#include "suclass.h"
#include "interp.h"
#include "globals.h"
#include "symbols.h"
#include "sustring.h"
#include "pack.h"
#include "cvt.h"

void SuClass::out(Ostream& out)
	{
	out << named.name() << " /* ";
	if (named.lib != "")
		out << named.lib << " ";
	out << "class";
	if (base != -1 && base != OBJECT)
		{
		out << " : ";
		if (globals(base) == 0)
			out << '_';
		else
			out << globals(base);
		}
	out << " */";
	}

Value SuClass::call(Value self, Value member, short nargs, short nargnames, ushort *argnames, int each)
	{
	static Value BASENAME("BaseName");

	if (member == INSTANTIATE)
		{
		SuObject* instance = new SuObject;
		instance->myclass = this;
		call(instance, NEW, nargs, nargnames, argnames, each);
		return instance;
		}
	else if (member == BASENAME)
		return new SuString(globals(base));
	else if (pmfn* p = basic_methods.find(member))
		return (this->*(*p))(nargs, nargnames, argnames, each);
	if (member == CALL)
		member = CALL_CLASS;
	if (tss_proc()->super)
		{
		short super = tss_proc()->super; tss_proc()->super = 0;
		return globals[super].call(self, member, nargs, nargnames, argnames, each);
		}
	if (Value x = get(member))
		return x.call(self, CALL, nargs, nargnames, argnames, each);
	else if (base)
		return globals[base].call(self, member, nargs, nargnames, argnames, each);
	else
		// for built-in classes that don't use RootClass
		method_not_found(type(), member);
	}

class Counter
	{
public:
	Counter()
		{ ++count; }
	~Counter()
		{ --count; }
	static int count;
	};
int Counter::count;

Value SuClass::getdata(Value member)
	{
	if (Value x = get(member))
		return x;
	else if (base)
		{
		Counter counter;
		if (Counter::count > 100)
			except("too many levels of derivation (possible cycle): " << this);
		return globals[base].getdata(member);
		}
	else
		return Value();
	}

size_t SuClass::packsize() const
	{
	if (*globals(base) == '_')
		except("can't pack class with _Name as base");
	return 1 + // PACK_CLASS
		packstrsize(globals(base)) +
		packnamesize(named) +
		SuObject::packsize();
	}

void SuClass::pack(char* buf) const
	{
	*buf++ = PACK_CLASS;
	verify(base > 0);
	buf += packstr(buf, globals(base));
	buf += packname(buf, named);
	SuObject::pack(buf);
	}

SuClass* SuClass::unpack(const gcstring& s)
	{
	const char* buf = s.buf() + 1; // skip PACK_CLASS
	short base = globals(unpackstr(buf));
	SuClass* c = new SuClass(base);
	buf += unpackname(buf, c->named);
	unpack2(gcstring(s.end() - buf, buf), c);
	return c;
	}

// RootClass --------------------------------------------------------

// the class for all "class-less" objects
// the base for all "base-less" classes
// theoretically the "basic" methods should be in here
// but for speed they are in class SuObject

Value RootClass::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	if (member == CALL || member == INSTANTIATE)
		{ // only used for Object(...)
		Value* args = GETSP() - nargs + 1;
		SuObject* ob;
		if (each >= 0)
			{
			verify(nargs == 1 && nargnames == 0);
			ob = args[0].object()->slice(each);
			}
		else
			{
			// create an object from the args
			ob = new SuObject;
			ob->myclass = this;
			// convert args to members
			short unamed = nargs - nargnames;
			// un-named
			int i;
			for (i = 0; i < unamed; ++i)
				ob->add(args[i]);
			// named
			verify(i >= nargs || argnames);
			for (int j = 0; i < nargs; ++i, ++j)
				ob->put(symbol(argnames[j]), args[i]);
			}
		return ob;
		}
	else
		{
		static ushort G_Objects = globals("Objects");
		Value Objects = globals.find(G_Objects);
		SuObject* ob;
		if (Objects && (ob = Objects.ob_if_ob()) && ob->has(member))
			return ob->call(self, member, nargs, nargnames, argnames, each);
		else
			return notfound(self, member, nargs, nargnames, argnames, each);
		}
	}

#include "func.h"

// this is separate so it can be used by other builtin classes
Value RootClass::notfound(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value DEFAULT("Default");

	if (member == NEW)
		return Value();
	else if (member == CALL_CLASS)
		// default for CallClass is instantiate
		return self.call(self, INSTANTIATE, nargs, nargnames, argnames, each);
	else if (member != DEFAULT)
		{
		// insert member before args
		argseach(nargs, nargnames, argnames, each); // have to expand first
		PUSH(0); // make space
		Value* sp = GETSP();
		int i;
		for (i = 0; i < nargs; ++i)
			sp[-i] = sp[-i - 1];
		sp[-i] = member;
		return self.call(self, DEFAULT, nargs + 1, nargnames, argnames, each);
		}
	else
		method_not_found(self.type(), ARG(0));
	}

