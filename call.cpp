// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "call.h"
#include "interp.h"
#include "globals.h"
#include "sustring.h"
#include "ostreamstr.h"
#include "exceptimp.h"

// used by QueryParser for extend macros
// used by Query expressions FunCall

Value call(const char* fname, Lisp<Value> args)
	{
	KEEPSP
	int nargs = 0;
	for (; ! nil(args); ++args, ++nargs)
		PUSH(*args);
	return docall(globals[fname], CALL, nargs);
	}

Value method_call(Value ob, const gcstring& method, Lisp<Value> args)
	{
	KEEPSP
	int nargs = 0;
	for (; ! nil(args); ++args, ++nargs)
		PUSH(*args);
	return docall(ob, new SuString(method), nargs);
	}

// used by sunapp
const char* trycall(const char* fn, char* arg, int* plen)
	{
	const char* str;
	try
		{
		gcstring gcstr;
		Value result = call(fn, lisp(Value(new SuString(arg))));
		if (result == SuFalse)
			return 0;
		gcstr = result.gcstr();
		if (plen)
			*plen = gcstr.size();
		str = gcstr.str();
		}
	catch (const Except& e)
		{
		// TODO catch block return ?
		OstreamStr oss;
		oss << fn << "(" << arg << ") => " << e;
		str = oss.str();
		if (plen)
			*plen = oss.size();
		}
	return str;
	}
