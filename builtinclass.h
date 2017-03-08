#ifndef BUILTINCLASS_H
#define BUILTINCLASS_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2003 Suneido Software Corp.
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

#include "func.h"
#include "interp.h"
#include "sustring.h"
#include "suobject.h"
#include "builtinargs.h"
#include "ctype.h"

template <class T> class BuiltinMethod : public SuValue
	{
public:
	typedef Value (T::*MemFun)(BuiltinArgs&);
	BuiltinMethod(T* o, MemFun f) : ob(o), fn(f)
		{ }
	virtual void out(Ostream& os)
		{ os << "/* builtin method */"; }
	virtual Value call(Value self, Value member,
		short nargs, short nargnames, ushort *argnames, int each)
		{
		if (member == CALL)
			{
			BuiltinArgs args(nargs, nargnames, argnames, each);
			return (ob->*fn)(args);
			}
		method_not_found("builtin-method", member);
		}
private:
	T* ob;
	MemFun fn;
	};

template <class T> struct Method
	{
	typedef class Value (T::*MemFun)(BuiltinArgs&);
	Method(const char* n, MemFun m) : name(n), method(m)
		{ }
	Value name;
	MemFun method;
	};

template <class T> const char* builtintype()
	{
	const char* s = typeid(T).name();
	while (isdigit(*s))
		++s; // for gcc
	if (has_prefix(s, "class "))
		s += 6;
	if (has_prefix(s, "Su"))
		s += 2;
	return s;
	}

template <class T> class BuiltinInstance : public T
	{
	typedef Value (T::*MemFun)(BuiltinArgs&);
	virtual Value call(Value self, Value member,
		short nargs, short nargnames, ushort *argnames, int each)
		{
		if (MemFun m = find(member))
			{
			BuiltinArgs args(nargs, nargnames, argnames, each);
			return (this->*m)(args);
			}
		method_not_found(builtintype<T>(), member);
		}
	virtual Value getdata(Value member)
		{
		if (MemFun m = find(member))
			return new BuiltinMethod<T>(this, m);
		except("member not found: " << member);
		}
	static MemFun find(Value member)
		{
		for (Method<T>* m = T::methods(); m->method; ++m)
			if (member == m->name)
				return m->method;
		return 0;
		}
	};

template <class T> class BuiltinClass : public SuValue
	{
public:
	char* params;

	BuiltinClass(char* p) : params(p)
		{}

	virtual Value call(Value self, Value member,
		short nargs, short nargnames, ushort *argnames, int each)
		{
		BuiltinArgs args(nargs, nargnames, argnames, each);
		static Value Members("Members");
		if (member == CALL)
			return callclass(args);
		else if (member == INSTANTIATE)
			return instantiate(args);
		else if (member == PARAMS && params != nullptr)
			return new SuString(params);
		else if (member == Members)
			{
			args.usage("usage: .Members()");
			args.end();
			SuObject* ob = new SuObject();
			for (Method<T>* m = T::methods(); m->method; ++m)
				ob->add(m->name);
			return ob;
			}
		else
			method_not_found(builtintype<T>(), member);
		}
	virtual void out(Ostream& os)
		{ os << "/* builtin class */"; }
	Value instantiate(BuiltinArgs&)
		{ return new T; }
	Value callclass(BuiltinArgs& args)
		{ return instantiate(args); }
	const char* type() const
		{ return "BuiltinClass"; }
	};

#endif
