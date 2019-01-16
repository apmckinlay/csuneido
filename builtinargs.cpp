// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

/*
This is the current preferred method of argument handling
for built-in functions/methods that require more than BUILTIN
Used by BuiltinClass
- does argseach
- handles getting arguments from named or unnamed
- handles default argument values
- must get arguments in order
- can handle variable args with getNext and curName
e.g.
	BuiltinArgs args(nargs, nargnames, argnames, each);
	args.usage("...");
	char* first = args.getstr("first");
	Value second = args.getValue("second", SuFalse);
	args.end();
for no arguments you can do:
	args.usage("...").end();
*/

// TODO cache pointer to args

#include "builtinargs.h"
#include "func.h"
#include "interp.h"
#include "except.h"

// ArgsIter ---------------------------------------------------------

ArgsIter::ArgsIter(short n, short na, short* an, int e)
	: nargs(n), nargnames(na), argnames(an), each(e) {
	if (each >= 0) {
		verify(nargs == 1);
		verify(nargnames == 0);
		ob = ARG(0).object();
		nargs = ob->size() - each;
		nargnames = ob->mapsize();
		oi.emplace(ob->begin());
		if (each == 1 && ob->vecsize() > 0)
			++*oi;
	}
	unnamed = nargs - nargnames;
}

Value ArgsIter::arg(int a) const {
	verify(a < unnamed || !ob);
	return ob ? ob->get(each + a) : ARG(a);
}

Value ArgsIter::next() {
	curname = Value();
	if (i >= nargs)
		return Value();
	if (ob) {
		auto p = **oi;
		if (i >= unnamed)
			curname = p.first;
		++*oi;
		++i;
		return p.second;
	} else {
		if (i >= unnamed)
			curname = symbol(argnames[i - unnamed]);
		return arg(i++);
	}
}

// takes the next unnamed
// or if no unnamed then looks for a matching named
Value ArgsIter::getval(const char* name) {
	if (Value x = getNextUnnamed())
		return x;
	return getNamed(name);
}

Value ArgsIter::getNamed(const char* name) {
	Value sym = symbol(name);
	Value x;
	if (ob)
		x = ob->get(sym);
	else {
		for (int j = 0; j < nargnames; ++j)
			if (argnames[j] == sym.symnum())
				x = arg(unnamed + j);
	}
	if (x)
		taken.add(sym);
	return x;
}

Value ArgsIter::getNextUnnamed() {
	return i < unnamed ? next() : Value();
}

bool ArgsIter::hasMoreUnnamed() const {
	return i < unnamed;
}

Value ArgsIter::getNext() {
	Value x;
	do
		x = next();
	while (x && taken.has(curname));
	return x;
}

// call with the remaining arguments
Value ArgsIter::call(Value fn, Value self, Value method) {
	verify(i <= unnamed);
	verify(taken.empty());
	return ob ? fn.call(self, method, 1, 0, nullptr, i + each)
			  : fn.call(self, method, nargs - i, nargnames, argnames, -1);
}

// BuiltinArgs ------------------------------------------------------

Value BuiltinArgs::getValue(const char* name) {
	Value val = getval(name);
	if (!val)
		exceptUsage();
	return val;
}

void BuiltinArgs::ckndef() const {
	verify(!def);
}

void BuiltinArgs::end() const {
	if (ai.hasMoreUnnamed())
		exceptUsage();
}

void BuiltinArgs::exceptUsage() const {
	except("usage: " << msg1 << msg2 << msg3);
}
