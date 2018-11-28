// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "func.h"
#include "interp.h"
#include "symbols.h"
#include "suclass.h"
#include "suobject.h"
#include <cctype>
#include "ostreamstr.h"
#include "sustring.h"
#include "fatal.h"
#include "catstr.h"
#include "opcodes.h"

Value Func::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	if (member == PARAMS)
		return new SuString(params());
	else
		method_not_found("Function", member);
}

char* Func::params() const {
	OstreamStr out;
	out << "(";
	short j = 0;
	for (int i = 0; i < nparams; ++i) {
		if (i != 0)
			out << ",";
		if (i == nparams - rest)
			out << "@";
		if (flags && (flags[i] & DYN))
			out << "_";
		out << symstr(locals[i]);
		if (i >= nparams - ndefaults - rest && i < nparams - rest)
			out << "=" << literals[j++];
	}
	out << ")";
	return out.str();
}

void Func::args(short nargs, short nargnames, short* argnames, int each) {
	Value* args = GETSP() - nargs + 1;
	short unamed = nargs - nargnames - (each == -1 ? 0 : 1);
	short i, j;

	if (!rest && unamed > nparams)
		except("too many arguments to " << this);

	verify(!rest || nparams == 1);    // rest must be only param
	verify(each == -1 || nargs == 1); // each must be only arg

	if (nparams > nargs)
		// expand stack (before filling it)
		SETSP(GETSP() + nparams - nargs);

	if (each != -1 && rest) {
		args[0] = args[0].object()->slice(each);
	} else if (rest) {
		// put args into object
		SuObject* ob = new SuObject();
		// un-named
		for (i = 0; i < unamed; ++i)
			ob->add(args[i]);
		// named
		for (j = 0; i < nargs; ++i, ++j)
			ob->put(symbol(argnames[j]), args[i]);
		args[0] = ob;
	} else if (each != -1) {
		SuObject* ob = args[0].object();

		if (ob->vecsize() > nparams + each)
			except("too many arguments to " << this << " vecsize "
											<< ob->vecsize() << " each " << each
											<< " nparams " << nparams);

		// un-named members
		for (i = 0; i < nparams; ++i)
			args[i] = ob->get(i + each);
		// named members
		verify(locals);
		for (i = 0; i < nparams; ++i)
			if (Value x = ob->get(symbol(locals[i])))
				args[i] = x;
	} else if (nargnames > 0) {
		// shuffle named args to match params
		const int maxargnames = 100;
		Value tmp[maxargnames];

		// move named args aside
		verify(nargnames < maxargnames);
		for (i = 0; i < nargnames; ++i)
			tmp[i] = args[unamed + i];

		// initialized remaining params
		for (i = unamed; i < nparams; ++i)
			args[i] = Value();

		// fill in params with named args
		verify(locals);
		for (i = 0; i < nparams; ++i)
			for (j = 0; j < nargnames; ++j)
				if (locals[i] == argnames[j])
					args[i] = tmp[j];
	} else {
		// initialized remaining params
		for (i = unamed; i < nparams; ++i)
			args[i] = Value();
	}

	// fill in dynamic implicits
	if (flags) {
		for (i = 0; i < nparams; ++i)
			if (!args[i] && (flags[i] & DYN)) {
				int sn = ::symnum(CATSTRA("_", symstr(locals[i])));
				args[i] = dynamic(sn);
			}
	}

	// fill in defaults
	if (ndefaults > 0) {
		verify(literals);
		for (j = nparams - ndefaults, i = 0; i < ndefaults; ++i, ++j)
			if (!args[j])
				args[j] = literals[i];
	}

	if (nargs > nparams)
		// shrink stack (after processing args)
		SETSP(GETSP() + nparams - nargs);

	// check that all parameters now have values
	verify(locals);
	for (i = 0; i < nparams; ++i)
		if (!args[i])
			except("missing argument(s) to " << this);
}

void noargs(short nargs, short nargnames, short* argnames, int each,
	const char* usage) {
	argseach(nargs, nargnames, argnames, each);
	if (nargs > nargnames)
		except("usage: " << usage);
}

// expand @args
void argseach(short& nargs, short& nargnames, short*& argnames, int& each) {
	if (each < 0)
		return;
	verify(nargs == 1);
	verify(nargnames == 0);
	SuObject* ob = POP().object();
	int i = 0;
	nargs = 0;
	int vs = ob->vecsize();
	argnames = new short[ob->mapsize()];
	for (auto iter = ob->begin(); iter != ob->end(); ++iter, ++i) {
		if (i < each)
			continue;
		if (i >= vs) // named members
			argnames[nargnames++] = (*iter).first.symnum();
		PUSH((*iter).second);
		++nargs;
	}
	each = -1;
}

void Func::out(Ostream& out) const {
	auto name = named.name();
	if (name.size())
		out << name << " ";
	out << "/* ";
	gcstring lib = named.library();
	if (lib != "")
		out << lib << " ";
	if (isMethod)
		out << "method */";
	else
		out << "function */";
}

void Func::show(Ostream& os) const {
	os << "function" << params();
}

#include "globals.h"
#include "scanner.h"

BuiltinFunc::BuiltinFunc(const char* name, const char* paramstr, BuiltinFn f) {
	pfn = f;
	ndefaults = 0;
	rest = false;
	literals = nullptr;
	nparams = 0;

	verify(isupper(*name));
	if (name[strlen(name) - 1] == 'Q') {
		char* s = STRDUPA(name);
		s[strlen(name) - 1] = '?';
		name = s;
	}
	named.num = globals(name);

	Scanner scanner(paramstr);
	verify(scanner.next() == '(');
	const int maxparams = 20;
	short params[maxparams];
	Value defaults[maxparams];
	int token = scanner.next();
	while (token != ')') {
		verify(token == T_IDENTIFIER);
		verify(nparams < maxparams);
		params[nparams++] = ::symnum(scanner.value);
		token = scanner.next();
		if (token == I_EQ) {
			Value x;
			token = scanner.next();
			if (token == T_NUMBER)
				x = SuNumber::literal(scanner.value);
			else if (token == T_STRING)
				x = new SuString(scanner.value, scanner.len);
			else if (token == T_IDENTIFIER && scanner.keyword == K_TRUE)
				x = SuTrue;
			else if (token == T_IDENTIFIER && scanner.keyword == K_FALSE)
				x = SuFalse;
			else
				fatal("invalid parameters for Primitive:", paramstr);
			defaults[ndefaults++] = x;
			token = scanner.next();
		} else if (ndefaults > 0)
			fatal("invalid parameters for Primitive:", paramstr);
		if (token == ',')
			token = scanner.next();
	}

	locals = new short[nparams];
	if (ndefaults > 0) {
		literals = new Value[ndefaults];
		memcpy(literals, defaults, ndefaults * sizeof(Value));
	}
	memcpy(locals, params, nparams * sizeof(short));
}

Value BuiltinFunc::call(Value self, Value member, short nargs, short nargnames,
	short* argnames, int each) {
	if (member != CALL)
		return Func::call(self, member, nargs, nargnames, argnames, each);
	args(nargs, nargnames, argnames, each);
	Framer frame(this, self);
	return pfn();
}

void BuiltinFunc::out(Ostream& out) const {
	out << named.name() << " /* builtin function */";
}
