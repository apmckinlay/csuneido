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

#include "suinstance.h"
#include "suclass.h"
#include "suvalue.h"
#include "interp.h"
#include "builtinargs.h"
#include "sublock.h"
#include "eqnest.h"

Value SuInstance::getdata(Value m)
	{
	if (Value* pv = data.find(m))
		return *pv;
	else
		return val_cast<SuClass*>(myclass)->get2(this, m);
	}

void SuInstance::putdata(Value m, Value v)
	{
	persist_if_block(v);
	data[m] = v;
	}

Value SuInstance::call(Value self, Value member,
	short nargs, short nargnames, short* argnames, int each)
	{
	static Value COPY("Copy");
	static Value DELETE("Delete");

	if (member == INSTANTIATE)
		except("can't create instance of instance");

	if (member == CALL)
		member = CALL_INSTANCE;
	if (tls().proc->super != NOSUPER)
		return callSuper(self, member, nargs, nargnames, argnames, each);
	if (auto f = method(member)) // methods common to instance and class
		{
		BuiltinArgs args(nargs, nargnames, argnames, each);
		return (this->*f)(args);
		}
	if (member == COPY)
		return Copy(nargs, nargnames, argnames, each);
	if (member == DELETE)
		return Delete(nargs, nargnames, argnames, each);
	return const_cast<Value*>(&myclass)->call(self, member, nargs, nargnames, argnames, each);
	}

bool SuInstance::eq(const SuValue& x) const
	{
	if (this == &x)
		return true;
	if (auto that = dynamic_cast<const SuInstance*>(&x))
		{
		if (myclass != that->myclass)
			return false;
		if (EqNest::has(this, &x))
			return true;
		EqNest eqnest(this, &x);
		return data == that->data;
		}
	return false;
	}

size_t SuInstance::hashfn() const
	{
	size_t hash = hashcontrib();
	if (data.size() <= 5)
		for (auto[key, val] : data)
			hash = 31 * hash + (key.hashcontrib() ^ val.hashcontrib());
	return hash;
	}

size_t SuInstance::hashcontrib() const
	{
	return myclass.hashcontrib() + 31 * data.size();
	}

void SuInstance::out(Ostream& os) const
	{
	if (const char* s = toString())
		os << s;
	else if (auto n = myclass.get_named())
		os << n->name() << "()";
	else
		os << "/* instance */";
	}

gcstring SuInstance::to_gcstr() const
	{
	if (const char* s = toString())
		return s;
	else
		return SuValue::to_gcstr(); // throw "can't convert"
	}

Value SuInstance::Copy(short nargs, short nargnames, short* argnames, int each)
	{
	NOARGS("instance.Copy()");
	SuInstance* cp = new SuInstance(myclass);
	cp->data = data.copy();
	return cp;
	}

Value SuInstance::Delete(short nargs, short nargnames, short* argnames, int each)
	{
	//FIXME: duplicate of SuObject::Delete
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("instance.Delete(member ...) or .Delete(all:)");
	if (Value all = args.getNamed("all"))
		{
		args.end();
		if (all == SuTrue)
			data.clear();
		}
	else
		{
		while (Value m = args.getNextUnnamed())
			data.erase(m);
		}
	return this;
	}

const char* SuInstance::toString() const
	{
	static Value ToString("ToString");

	SuClass* c = force<SuClass*>(myclass);
	if (c->hasMethod(ToString))
		{
		KEEPSP
		if (Value x = c->call(const_cast<SuInstance*>(this), ToString,
			0, 0, nullptr, -1))
			if (auto s = x.str_if_str())
				return s;
		except("ToString should return a string");
		}
	return nullptr;
	}
