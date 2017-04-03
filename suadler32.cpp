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

#include "builtinclass.h"
#include "suboolean.h"
#include "gcstring.h"
#include "checksum.h"

class SuAdler32 : public SuValue
	{
public:
	SuAdler32() : value(1)
		{ }
	void out(Ostream& os) const override
		{ os << "Adler32()"; }
	static Method<SuAdler32>* methods()
		{
		static Method<SuAdler32> methods[] =
			{
			Method<SuAdler32>("Update", &SuAdler32::Update),
			Method<SuAdler32>("Value", &SuAdler32::ValueFn),
			Method<SuAdler32>("", nullptr)
			};
		return methods;
		}
	const char* type() const override
		{ return "Adler32"; }
	void update(const gcstring& gcstr);
	ulong value;
private:
	Value Update(BuiltinArgs&);
	Value ValueFn(BuiltinArgs&);
	};

Value su_adler32()
	{
	static BuiltinClass<SuAdler32> suAdler32Class("(@strings)");
	return &suAdler32Class;
	}

template<>
void BuiltinClass<SuAdler32>::out(Ostream& os) const
	{ os << "Adler32 /* builtin class */"; }

template<>
Value BuiltinClass<SuAdler32>::instantiate(BuiltinArgs& args)
	{
	args.usage("usage: new Adler32()");
	args.end();
	return new BuiltinInstance<SuAdler32>();
	}

template<>
Value BuiltinClass<SuAdler32>::callclass(BuiltinArgs& args)
	{
	args.usage("usage: Adler32(@strings)");
	SuAdler32* a = new BuiltinInstance<SuAdler32>();
	if (! args.hasUnnamed())
		return a;
	while (Value x = args.getNextUnnamed())
		a->update(x.gcstr());
	return a->value;
	}

Value SuAdler32::Update(BuiltinArgs& args)
	{
	args.usage("usage: adler32.Update(string)");
	gcstring s = args.getgcstr("string");
	args.end();

	update(s);
	return this;
	}

void SuAdler32::update(const gcstring& s)
	{
	value = checksum(value, s.ptr(), s.size());
	}

Value SuAdler32::ValueFn(BuiltinArgs& args)
	{
	args.usage("usage: adler32.Value()");
	args.end();
	return value;
	}
