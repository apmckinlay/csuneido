/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2004 Suneido Software Corp. 
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

#include <locale.h>
#include "value.h"
#include "interp.h"
#include "sustring.h"
#include "prim.h"
#if defined(_MSC_VER) && _MSC_VER <= 1200
//#include <locale>
#endif

void scanner_locale_changed();

Value su_setlocale()
	{
	const int nargs = 2;
	int category = ARG(0).integer();
	char* locale = ARG(1).str();
	if (0 == strcmp(locale, "0"))
		locale = NULL;

	char* s = setlocale(category, locale);
#if defined(_MSC_VER) && _MSC_VER <= 1200
//	std::locale::global(std::locale(locale));
#endif

	scanner_locale_changed();

	return s ? Value(new SuString(s)) : Value(0);
	}
PRIM(su_setlocale, "SetLocale(category, locale=0)");
