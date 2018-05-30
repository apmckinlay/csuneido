/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2017 Suneido Software Corp.
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

#include "membase.h"
#include "globals.h"
#include "builtinargs.h"
#include "sumethod.h"
#include "suobject.h"
#include "sufunction.h"
#include "interp.h"

MemFun<MemBase> MemBase::method(Value member)
	{
	static Meth<MemBase> meths[] =
		{
		{ "Size", &MemBase::Size },
		{ "GetDefault", &MemBase::GetDefault },
		{ "Member?", &MemBase::MemberQ },
		{ "Members", &MemBase::Members },
		{ "Readonly?", &MemBase::ReadonlyQ },
		{ "Base", &MemBase::Base },
		{ "Base?", &MemBase::BaseQ },
		{ "Eval", &MemBase::Eval },
		{ "Eval2", &MemBase::Eval2 },
		{ "Method?", &MemBase::MethodQ },
		{ "MethodClass", &MemBase::MethodClass },
		};
	for (auto m : meths)
		if (m.name == member)
			return m.fn;
	return nullptr;
	}

Value MemBase::callSuper(Value self, Value member,
	short nargs, short nargnames, ushort *argnames, int each)
	{
	short super = tls().proc->super;
	tls().proc->super = NOSUPER;
	verify(super != NOSUPER);
	if (super)
		return globals[super].call(self, member, nargs, nargnames, argnames, each);
	if (member == NEW)
		{
		if (nargs > nargnames) // as usual, ignore excess named arguments
			except("too many arguments to New");
		return Value();
		}
	//TODO don't allow super call without base
	method_not_found(type(), member);
	}

// Suneido methods --------------------------------------------------

Value MemBase::Base(BuiltinArgs& args)
	{
	args.usage("class.Base()").end();
	Value base = parent();
	return base ? base : SuFalse;
	}

// applies the finder function to this and all its parents
// continues while finder returns Value()
template <typename Finder>
Value MemBase::lookup(Finder finder)
	{
	MemBase* mb = this;
	for (int i = 0; i < 100; ++i)
		{
		if (Value x = finder(mb))
			return x;
		mb = val_cast<MemBase*>(mb->parent());
		if (! mb)
			return Value();
		}
	except("too many levels of derivation (possible cycle): " << this);
	}

Value MemBase::BaseQ(BuiltinArgs& args)
	{
	args.usage("class.Base?(value)");
	Value value = args.getValue("value");
	args.end();
	return hasBase(value) ? SuTrue : SuFalse;
	}

bool MemBase::hasBase(Value value)
	{
	return !!lookup([value](MemBase* mb) -> Value
		{ return mb == value ? SuTrue : Value();  });
	}

Value MemBase::Eval(BuiltinArgs& args)
	{
	args.usage("class.Eval(function, args ...)");
	Value fn = args.getNext();
	if (SuMethod* meth = val_cast<SuMethod*>(fn))
		fn = meth->fn();
	return args.call(fn, this, CALL);
	}

Value MemBase::Eval2(BuiltinArgs& args)
	{
	Value result = Eval(args);
	SuObject* ob = new SuObject;
	if (result)
		ob->add(result);
	return ob;
	}

Value MemBase::GetDefault(BuiltinArgs& args) // see also: SuObject.GetDefault
	{
	args.usage("class.GetDefault?(member, default)");
	Value mem = args.getValue("member");
	Value def = args.getValue("block");
	args.end();
	if (Value x = getdata(mem))
		return x;
	if (0 != strcmp(def.type(), "Block"))
		return def;
	KEEPSP
	return def.call(def, CALL);
	}

Value MemBase::MemberQ(BuiltinArgs& args)
	{
	args.usage("class.Member?(name)");
	Value m = args.getValue("member");
	args.end();
	Value x = lookup([m](MemBase* mb) -> Value
		{ return mb->data.find(m) ? SuTrue : Value(); });
	return x ? SuTrue : SuFalse;
	}

Value MemBase::Members(BuiltinArgs& args)
	{
	args.usage("class.Members() or class.Members(all:)");
	bool all = args.getValue("all", SuFalse).toBool();
	args.end();
	SuObject* ob = new SuObject();
	if (all)
		{
		lookup([ob](MemBase* mb) -> Value
			{ mb->addMembersTo(ob); return Value(); });
		ob->sort();
		ob->unique();
		}
	else
		addMembersTo(ob);
	return ob;
	}

void MemBase::addMembersTo(SuObject* ob)
	{
	for (auto [key, val] : data)
		{
		(void)val;
		ob->add(key);
		}
	}

Value MemBase::MethodQ(BuiltinArgs& args)
	{
	args.usage("class.MethodQ(name)");
	Value x = method_lookup(args);
	return x && x != SuFalse ? SuTrue : SuFalse;
	}

Value MemBase::MethodClass(BuiltinArgs& args)
	{
	args.usage("class.MethodClass(name)");
	Value x = method_lookup(args);
	return x ? x : SuFalse;
	}

Value MemBase::method_lookup(BuiltinArgs& args)
	{
	Value m = args.getValue("name");
	args.end();
	return method_class(m);
	}

Value MemBase::method_class(Value m)
	{
	return lookup([m](MemBase* mb) -> Value
		{
		if (Value* pv = mb->data.find(m))
			return val_cast<SuFunction*>(*pv) ? mb : SuFalse;
		return Value();
		});
	}

Value MemBase::ReadonlyQ(BuiltinArgs& args)
	{
	args.usage(".Readonly?()").end();
	return readonly();
	}

Value MemBase::Size(BuiltinArgs& args)
	{
	args.usage(".Size()").end();
	return data.size();
	}
