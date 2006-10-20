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

#include "literal.h"

#include "sustring.h"
#include "suboolean.h"
#include "sunumber.h"
#include "sudate.h"
#include "symbols.h"
#include <ctype.h> // for tolower

Value literal(const char* s)
	{
	if (*s == '"' || *s == '\'')
		return SuString::literal(s);
	else if (tolower(*s) == 't' || tolower(*s) == 'f')
		return SuBoolean::literal(s);
	else if (s[0] == '#')
		return SuDate::literal(s);
	else
		return SuNumber::literal(s);
	}

// tests ============================================================

#include "testing.h"
#include "ostreamstr.h"

class test_literal : public Tests
	{
	TEST(0, boolean_text)
		{
		char* s;
		Value x;
		s = "true";
		x = SuBoolean::literal(s);
		asserteq(outstr(x), s);
		s = "false";
		x = SuBoolean::literal(s);
		asserteq(outstr(x), s);
		}
	TEST(1, string_text)
		{
		char* s = "\"hello\nworld\1\2\"";
		Value x = SuString::literal(s);
		asserteq(outstr(x), s);
		}
	TEST(2, number_text)
		{
		test1num("0");
		test1num("1");
		test1num("-1");
		test1num("123");
		test1num("-123");
		test1num("123.456");
		test1num("-123.456");
		test1num("1.23e45");
		test1num("-1.23e-45");
		}
	void test1num(char* s)
		{
		Value x = SuNumber::literal(s);
		asserteq(outstr(x), s);
		}
	gcstring outstr(Value x)
		{
		OstreamStr os;
		os << x;
		return os.str();
		}
	TEST(3, literal)
		{
		asserteq(literal("true"), SuTrue);
		asserteq(literal("false"), SuFalse);
		asserteq(literal("'false'"), Value("false"));
		asserteq(literal("\"false\""), Value("false"));
		asserteq(literal("#20020912"), Value(SuDate::literal("#20020912")));
		asserteq(literal("20020912"), Value(SuNumber::literal("20020912")));
		}
	};
REGISTER(test_literal);
