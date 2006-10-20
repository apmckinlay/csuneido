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

#include "scanner.h"
#include "suvalue.h"
#include "interp.h"
#include "symbols.h"
#include "suboolean.h"
#include "sustring.h"
#include "prim.h"

class SuScanner : public SuValue
	{
public:
	explicit SuScanner(char* s) : scanner(s)
		{ }
	void out(Ostream& os)
		{ os << "Scanner"; }
	Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
private:
	Scanner scanner;
	int token;
	};

Value suscanner()
	{
	return new SuScanner(proc->stack.top().str());
	}
PRIM(suscanner, "Scanner(string)");

Value SuScanner::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value NEXT("Next");
	static Value POSITION("Position");
	static Value TYPE("Type");
	static Value N_TEXT("Text");
	static Value VALUE("Value");
	static Value KEYWORD("Keyword");
	static Value ITER("Iter");

	if (member == NEXT)
		{
		token = scanner.nextall();
		if (token == -1)
			return this;
		else
			return new SuString(scanner.source + scanner.prev, scanner.si - scanner.prev);
		}
	else if (member == TYPE)
		return token;
	else if (member == N_TEXT)
		return new SuString(scanner.source + scanner.prev, scanner.si - scanner.prev);
	else if (member == VALUE)
		return new SuString(scanner.value);
	else if (member == POSITION)
		return scanner.si;
	else if (member == KEYWORD)
		return scanner.keyword == 0 ? 0 : scanner.keyword - KEYWORDS;
	else if (member == ITER)
		return this;
	else
		unknown_method("Scanner", member);
	}

