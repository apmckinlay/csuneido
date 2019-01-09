// Copyright (c) 2018 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "porttest.h"
#include "value.h"
#include "sustring.h"
#include "compile.h"
#include "interp.h"
#include "ostreamstr.h"
#include "exceptimp.h"
#include "func.h"
#include "globals.h"
#include "htbl.h"

static Value def() {
	const int nargs = 2;
	auto name = ARG(0).str();
	auto value = ARG(1);
	if (auto s = value.str_if_str()) {
		value = compile(s, name);
		if (Named* n = const_cast<Named*>(value.get_named())) {
			n->num = globals(name);
		}
	}
	globals.put(globals(name), value);
	return Value();
}

extern Hmap<int, Value> builtins;

#define TOVAL(i) (str[i] ? new SuString(args[i]) : compile(args[i].str()))

PORTTEST(method) {
	Value ob = TOVAL(0);
	auto method = new SuString(args[1]);
	Value expected = TOVAL(args.size() - 1);
	KEEPSP
	for (int i = 2; i < args.size() - 1; ++i)
		PUSH(TOVAL(i));
	Value result = ob.call(ob, method, args.size() - 3);
	return result == expected ? nullptr : OSTR("got: " << result);
}

PORTTEST(execute) {
	if (!globals.get(globals("Def"))) {
		auto d = new BuiltinFunc("Def", "(name, value)", def);
		globals.put(globals("Def"), d);
	}
	gcstring expected = "**notfalse**";
	if (args.size() > 1)
		expected = args[1];
	Value result;
	bool ok;
	if (expected == "throws") {
		if (args[2].has_prefix("super")) {
			return nullptr; // SKIP
		}
		try {
			result = run(args[0].str());
			ok = false;
		} catch (Except& e) {
			result = new SuString(e.gcstr());
			ok = e.gcstr().has(args[2]);
		}
	} else if (expected == "**notfalse**") {
		result = run(args[0].str());
		ok = result != SuFalse;
	} else {
		result = run(args[0].str());
		Value expectedVal = compile(expected.str());
		ok = result == expectedVal;
	}
	if (!ok)
		return OSTR("got: " << result);
	return nullptr;
}
