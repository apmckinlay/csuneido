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
#include "suobject.h"
#include "suinstance.h"
#include "interp.h"
#include "globals.h"
#include "sustring.h"
#include "sufunction.h"
#include "sumethod.h"
#include "catstr.h"
#include <cctype>
#include "builtinargs.h"

void SuClass::put(Value m, Value x)
	{
	data[m] = x;
	}

Value SuClass::get(Value m) const // no inheritance
	{
	if (Value* pv = data.find(m))
		return *pv;
	return Value();
	}

void SuClass::out(Ostream& out) const
	{
	out << named.name() << " /* ";
	if (named.lib != "")
		out << named.lib << " ";
	out << "class";
	if (base)
		{
		out << " : ";
		if (globals(base) == nullptr)
			out << '_';
		else
			out << globals(base);
		}
	out << " */";
	}

Value SuClass::call(Value self, Value member,
	short nargs, short nargnames, ushort *argnames, int each)
	{
	if (member == INSTANTIATE)
		{
		Value instance = new SuInstance(this);
		call(instance, NEW, nargs, nargnames, argnames, each);
		return instance;
		}
	if (auto f = method(member)) // builtin methods
		{
		BuiltinArgs args(nargs, nargnames, argnames, each);
		return (this->*f)(args);
		}
	if (member == CALL)
		member = CALL_CLASS;
	if (tls().proc->super != NOSUPER)
		return callSuper(self, member, nargs, nargnames, argnames, each);

	if (Value x = get3(member))
		return x.call(self, CALL, nargs, nargnames, argnames, each);

	if (member == CALL_CLASS) // default for CallClass is Instantiate
		return call(self, INSTANTIATE, nargs, nargnames, argnames, each);

	if (member == NEW) // no more New in inheritance (since get3 failed)
		{
		argseach(nargs, nargnames, argnames, each);
		if (nargs > nargnames) // as usual, excess named arguments are ignored
			except("too many arguments to New");
		return Value();
		}

	static UserDefinedMethods udm("Objects");
	if (Value c = udm(member))
		return c.call(self, member, nargs, nargnames, argnames, each);

	static Value DEFAULT("Default");
	if (member != DEFAULT)
		{
		argseach(nargs, nargnames, argnames, each);
		// insert member before args
		PUSH(0); // make space
		Value* sp = GETSP();
		int i;
		for (i = 0; i < nargs; ++i)
			sp[-i] = sp[-i - 1];
		sp[-i] = member;
		return self.call(self, DEFAULT, nargs + 1, nargnames, argnames, each);
		}
	method_not_found(type(), member);
	}

UserDefinedMethods::UserDefinedMethods(const char* name) : gnum(globals(name))
	{
	}

class Database;
extern Database* thedb;
bool isclient();

Value UserDefinedMethods::operator()(Value member) const
	{
	if (thedb || isclient()) // don't access database in tests
		if (SuClass* c = val_cast<SuClass*>(globals.find(gnum)))
			if (c->hasMethod(member))
				return c;
	return Value();
	}

Value SuClass::getdata(Value member)
	{
	return get2(this, member);
	}

// 'self' will be different from 'this' when called by SuInstance.get2
Value SuClass::get2(Value self, Value member) // handles binding and getters
	{
	if (Value x = get3(member))
		{
		if (SuFunction* sufn = val_cast<SuFunction*>(x))
			return new SuMethod(self, member, sufn);
		return x;
		}
	if (has_getter)
		{
		static Value Get_("Get_");
		if (Value method = get3(Get_))
			{
			KEEPSP
			PUSH(member);
			return method.call(self, CALL, 1);
			}
		else
			has_getter = false; // avoid future attempts
		}
	if (const char* s = member.str_if_str())
		{
		Value getter = new SuString(CATSTRA(islower(*s) ? "get_" : "Get_", s));
		if (Value method = get3(getter))
			{
			KEEPSP
			return method.call(self, CALL);
			}
		}
	return Value();
	}

Value SuClass::get3(Value member) // handles inheritance
	{
	SuClass* c = this;
	for (int i = 0; i < 100; ++i)
		{
		if (Value x = c->get(member))
			return x;
		if (! c->base)
			return Value();
		c = force<SuClass*>(globals[c->base]);
		}
	except("too many levels of derivation (possible cycle): " << this);
	}

Value SuClass::parent()
	{
	return base ? globals[base] : Value();
	}

//-------------------------------------------------------------------

#include "testing.h"

TEST(suclass_construct)
	{
	val_cast<SuClass*>(run("class { }"));
	val_cast<SuInstance*>(run("c = class { }; new c"));
	val_cast<SuInstance*>(run("c = class { }; c()"));
	assert_eq(Value(123), run("c = class { CallClass() { 123 } }; c()"));
	assert_eq(run("#(Foo, 123)"),
		run("c = class { Default(@args) { args } }; c.Foo(123)"));
	assert_eq(run("#(Foo, 123)"),
		run("c = class { Default(@args) { args } }; c().Foo(123)"));
	run("new class{ New() { } }");
	assert_eq(Value(123),
		run("c = class { New(.x) { } F() { .x } }; c(123).F()"));
	assert_eq(Value("foo"),
		run("c = class { ToString() { 'foo' } }; Display(c())"));
	}
