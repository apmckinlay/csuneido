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

#include "func.h"
#include "interp.h"
#include "symbols.h"
#include "suclass.h"
#include "lisp.h"
#include <stdarg.h>
#include <ctype.h>
#include "ostreamstr.h"
#include "sustring.h"
#include "fatal.h"

Value Func::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value Params("Params");

	if (member == Params)
		return params();
	else
		unknown_method("function", member);
	return Value();
	}

Value Func::params()
	{
	OstreamStr out;
	out << "(";
	short j = 0;
	for (int i = 0; i < nparams; ++i)
		{
		if (i != 0)
			out << ",";
		if (i == nparams - rest)
			out << "@";
		out << symstr(locals[i]);
		if (i >= nparams - ndefaults - rest && i < nparams - rest)
			out << "=" << literals[j++];
		}
	out << ")";
	return new SuString(out.str());
	}

void Func::args(short nargs, short nargnames, ushort* argnames, int each)
	{
	Value* args = GETSP() - nargs + 1;
	short unamed = nargs - nargnames - (each == -1 ? 0 : 1);
	short i, j;
	
	if (! rest && unamed > nparams)
		except("too many arguments to " << this);

	verify(! rest || nparams == 1);	// rest must be only param
	verify(each == -1 || nargs == 1);	// each must be only arg

	if (nparams > nargs)
		// expand stack (before filling it)
		SETSP(GETSP() + nparams - nargs);

	if (each != -1 && rest)
		{
		args[0] = args[0].object()->slice(each);
		}
	else if (rest)
		{
		// put args into object
		SuObject* ob = new SuObject();
		// un-named
		for (i = 0; i < unamed; ++i)
			ob->add(args[i]);
		// named
		for (j = 0; i < nargs; ++i, ++j)
			ob->put(symbol(argnames[j]), args[i]);
		args[0] = ob;
		}
	else if (each != -1)
		{
		SuObject* ob = args[0].object();
		
		if (ob->vecsize() - each > nparams)
			except("too many arguments to " << this);
		
		// un-named members
		for (i = 0; i < nparams; ++i)
			args[i] = ob->get(i + each);
		// named members
		verify(locals);
		for (i = 0; i < nparams; ++i)
			if (Value x = ob->get(symbol(locals[i])))
				args[i] = x;
		}
	else if (nargnames > 0)
		{
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
		}
	else
		{
		// initialized remaining params
		for (i = unamed; i < nparams; ++i)
			args[i] = Value();
		}

	// fill in defaults
	if (ndefaults > 0)
		{
		verify(literals);
		for (j = nparams - ndefaults, i = 0; i < ndefaults; ++i, ++j)
			if (! args[j])
				args[j] = literals[i];
		}

	if (nargs > nparams)
		// shrink stack (after processing args)
		SETSP(GETSP() + nparams - nargs);

	// check that all parameters now have values
	verify(locals);
	for (i = 0; i < nparams; ++i)
		if (! args[i])
			except("missing argument(s) to " << this);
	}

void argseach(short& nargs, short& nargnames, ushort*& argnames, int& each)
	{
	if (each < 0)
		return ;
	verify(nargs == 1);
	verify(nargnames == 0);
	SuObject* ob = POP().object();
	int i = 0;
	nargs = 0;
	int vs = ob->vecsize();
	argnames = new ushort[ob->mapsize()];
	for (SuObject::iterator iter = ob->begin(); iter != ob->end(); ++iter, ++i)
		{
		if (i < each)
			continue ;
		if (i >= vs) // named members
			argnames[nargnames++] = (*iter).first.symnum();
		PUSH((*iter).second);
		++nargs;
		}
	each = -1;
	}

void Func::out(Ostream& out)
  	{
	out << named.name() << " /* " << named.library() << " function */";
/*
	short j = 0;
	for (short i = 0; i < nparams; ++i)
		{
		if (i != 0)
			out << ",";
		if (i == nparams - rest)
			out << "@";
		out << names(locals[i]);
		if (i >= nparams - ndefaults - rest && i < nparams - rest)
			out << "=" << literals[j++];
		}
	out << ")";
*/
	}

Primitive::Primitive(PrimFn f, ...)
   	{
	pfn = f;
	ndefaults = 0;
	rest = false;
	literals = 0;

	va_list ap;
	va_start(ap, f);
	const int maxparams = 20;
	short tmp[maxparams];
	char* s;
	for (nparams = 0; 0 != (s = va_arg(ap, char*)); ++nparams)
		{
		verify(nparams < maxparams);
		verify(! rest);
		if (*s == '@')
			{
			rest = true;
			++s;
			}
		tmp[nparams] = ::symnum(s);
		}
	va_end(ap);
	locals = new ushort[nparams];
	memcpy(locals, tmp, nparams * sizeof (ushort));
	}

#include "globals.h"
#include "scanner.h"

Primitive::Primitive(char* decl, PrimFn f)
	{
	pfn = f;
	ndefaults = 0;
	rest = false;
	literals = 0;
	nparams = 0;

	Scanner scanner(decl);
	verify(scanner.next() == T_IDENTIFIER);
	verify(isupper(scanner.value[0]));
	named.num = globals(scanner.value);
	verify(scanner.next() == '(');
	const int maxparams = 20;
	ushort params[maxparams];
	Value defaults[maxparams];
	int token = scanner.next();
	while (token != ')')
		{
		verify(token == T_IDENTIFIER);
		verify(nparams < maxparams);
		params[nparams++] = ::symnum(scanner.value);
		token = scanner.next();
		if (token == I_EQ)
			{
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
				fatal("invalid parameters for Primitive:", decl);
			defaults[ndefaults++] = x;
			token = scanner.next();
			}
		else if (ndefaults > 0)
			fatal("invalid parameters for Primitive:", decl);
		if (token == ',')
			token = scanner.next();
		}

	locals = new ushort[nparams];
	if (ndefaults > 0)
		{
		literals = new Value[ndefaults];
		memcpy(literals, defaults, ndefaults * sizeof (Value));
		}
	memcpy(locals, params, nparams * sizeof (ushort));
	}

Value Primitive::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	if (member != CALL)
		return Func::call(self, member, nargs, nargnames, argnames, each);
	args(nargs, nargnames, argnames, each);
	Framer frame(this, self);
	return pfn();
	}
