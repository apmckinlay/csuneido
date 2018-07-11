// Copyright (c) 2018 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "porttest.h"
#include "value.h"
#include "sustring.h"
#include "compile.h"
#include "interp.h"
#include "ostreamstr.h"
#include "exceptimp.h"

#define TOVAL(i) (str[i] ? new SuString(args[i]) : compile(args[i].str()))

PORTTEST(method)
	{
	Value ob = TOVAL(0);
	auto method = new SuString(args[1]);
	Value expected = TOVAL(args.size() - 1);
	KEEPSP
	for (int i = 2; i < args.size() - 1; ++i)
		PUSH(TOVAL(i));
	Value result = ob.call(ob, method, args.size() - 3);
	return result == expected ? nullptr : OSTR("got: " << result);
	}

PORTTEST(execute)
	{
	gcstring expected = "**notfalse**";
	if (args.size() > 1)
		expected = args[1];
	Value result;
	bool ok;
	if (expected == "throws")
		{
		try
			{
			result = run(args[0].str());
			ok = false;
			}
		catch (Except& e)
			{
			result = new SuString(e.gcstr());
			ok = e.gcstr().has(args[2]);
			}
		}
	else if (expected == "**notfalse**")
		{
		result = run(args[0].str());
		ok = result != SuFalse;
		}
	else
		{
		result = run(args[0].str());
		Value expectedVal = compile(expected.str());
		ok = result == expectedVal;
		}
	if (!ok)
		return OSTR("got: " << result);
	return nullptr;
	}
