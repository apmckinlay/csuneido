// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "sumethod.h"
#include "interp.h"
#include "sufunction.h"

void SuMethod::out(Ostream& os) const
	{
	if (sufn->named.lib != "" || sufn->named.parent)
		os << sufn;
	else
		os << "/* method " << sufn->named.name() << " */";
	}

bool SuMethod::eq(const SuValue& y) const
	{
	if (const SuMethod* m = dynamic_cast<const SuMethod*>(&y))
		return object.sameAs(m->object) && method == m->method;
	else
		return false;
	}

size_t SuMethod::hashfn() const
	{
	// can't use object.hash() because it will change if object changes
	return size_t(object.ptr()) ^ method.hash();
	}

const Named* SuMethod::get_named() const
	{
	return sufn->get_named();
	}

Value SuMethod::call(Value self, Value member, 
	short nargs, short nargnames, short* argnames, int each)
	{
	if (member == CALL)
		return object.call(object, method, nargs, nargnames, argnames, each);
	else
		// pass other methods e.g. Params through to the function
		return sufn->call(sufn, member, nargs, nargnames, argnames, each);
	}
