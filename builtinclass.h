#pragma once
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

#include "interp.h"
#include "sustring.h"
#include "suobject.h"
#include "builtinargs.h"
#include "ctype.h"
#include "gsl-lite.h"

/*
 * Usage:
 * define your class, inherit from SuValue
 * define methods() and optionally static_methods()
 * returning span of Method / StaticMethod
 * methods take BuiltinArgs&
 * specialize instantiate and optionally callclass (for the class)
 */

// a bound built-in method
template <class T> class BuiltinMethod : public SuValue
	{
public:
	typedef Value (T::*MemFun)(BuiltinArgs&);
	BuiltinMethod(T* o, MemFun f) : ob(o), fn(f)
		{ }
	void out(Ostream& os) const override
		{ os << "/* builtin method */"; }
	Value call(Value self, Value member,
		short nargs, short nargnames, uint16_t *argnames, int each) override
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

// method array entries
template <class T> struct Method
	{
	typedef class Value (T::*MemFun)(BuiltinArgs&);
	Value name;
	MemFun method;
	};

typedef Value (*StaticFun)(BuiltinArgs&);

// static method array entries
struct StaticMethod
	{
	Value name;
	StaticFun method;
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

// inherits from your class and implements common methods
template <class T> class BuiltinInstance : public T
	{
	typedef Value (T::*MemFun)(BuiltinArgs&);
	virtual Value call(Value self, Value member,
		short nargs, short nargnames, uint16_t *argnames, int each) override
		{
		if (MemFun m = find(member))
			{
			BuiltinArgs args(nargs, nargnames, argnames, each);
			return (this->*m)(args);
			}
		method_not_found(builtintype<T>(), member);
		}
	Value getdata(Value member) override
		{
		if (MemFun m = find(member))
			return new BuiltinMethod<T>(this, m);
		except("member not found: " << member);
		}
	static MemFun find(Value member)
		{
		for (auto m : T::methods())
			if (member == m.name)
				return m.method;
		return nullptr;
		}
	void out(Ostream& os) const override
		{
		os << "a" << builtintype<T>();
		}
	const char* type() const override
		{
		return builtintype<T>();
		}
	};

// handles call: CALL, INSTANTIATE, PARAMS, Members, 
// callclass defaults to instantiate
template <class T> class BuiltinClass : public SuValue
	{
public:
	explicit BuiltinClass(const char* p) : params(p)
		{}

	Value call(Value self, Value member,
		short nargs, short nargnames, uint16_t *argnames, int each) override
		{
		BuiltinArgs args(nargs, nargnames, argnames, each);
		static Value Members("Members");
		if (member == CALL)
			return callclass(args);
		else if (member == INSTANTIATE)
			return instantiate(args);
		else if (member == PARAMS && params)
			return new SuString(params);
		else if (member == Members)
			{
			args.usage(".Members()").end();
			SuObject* ob = new SuObject();
			// class doesn't really have instance methods, but Suneido classes do
			for (auto m : T::methods())
				ob->add(m.name);
			for (auto m : static_methods())
				ob->add(m.name);
			ob->add("Members");
			if (params)
				ob->add("Params");
			return ob;
			}
		else if (auto m = find(member))
			return (*m)(args);
		else
			method_not_found(builtintype<T>(), member);
		}
	static StaticFun find(Value member)
		{
		for (auto m : static_methods())
			if (member == m.name)
				return m.method;
		return nullptr;
		}
	static auto static_methods()
		{
		return gsl::span<StaticMethod>();
		}
	void out(Ostream& os) const override	
		{
		os << builtintype<T>() << " /* builtin class */"; 
		}
	static Value instantiate(BuiltinArgs&);
	static Value callclass(BuiltinArgs& args)
		{ return instantiate(args); }
	const char* type() const override
		{ return "BuiltinClass"; }

private:
	const char* params;
	};
