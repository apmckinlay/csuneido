// Copyright (c) 2004 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"
#include "interp.h"
#include "sustring.h"
#include "builtin.h"
#include <clocale>

void scanner_locale_changed();

BUILTIN(SetLocale, "(category, locale=0)")
	{
	const int nargs = 2;
	int category = ARG(0).integer();
	auto locale = ARG(1).str();
	if (0 == strcmp(locale, "0"))
		locale = nullptr;

	char* s = setlocale(category, locale);

	scanner_locale_changed();

	return s ? Value(new SuString(s)) : Value(0);
	}
