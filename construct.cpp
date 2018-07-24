// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "construct.h"
#include "suobject.h"
#include "interp.h"
#include "globals.h"
#include "suclass.h"
#include "sustring.h"
#include "catstr.h"

// Construct(class, suffix="") or Construct(#(class, args...), suffix="")
// if class is string, combines class + suffix to get class name
// then simply calls the class's constructor with the specified args

Construct::Construct() {
	named.num = globals("Construct");
}

Value Construct::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	if ((nargs != 1 && nargs != 2) || member != CALL)
		except("usage: Construct(string, suffix='') "
			   "or Construct(object, suffix='')");
	const char* suffix = nargs == 1 ? "" : ARG(1).str();
	Value c = ARG(0);
	SuObject* ob = c.ob_if_ob();
	if (ob) {
		if (val_cast<SuClass*>(c))
			ob = 0;
		else if (!((c = ob->get(0))))
			except("Construct: object requires member 0");
	}
	if (const char* s = c.str_if_str()) {
		if (!has_suffix(s, suffix))
			s = CATSTRA(s, suffix);
		c = globals[s];
	}
	if (nargs == 2)
		POP(); // suffix
	if (ob)
		// nargs=1, each=1 => c(@+1 ob)
		return c.call(c, INSTANTIATE, 1, 0, nullptr, 1);
	else {
		POP(); // class
		return c.call(c, INSTANTIATE);
	}
}
