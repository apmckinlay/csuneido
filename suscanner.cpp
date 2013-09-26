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

#include "builtinclass.h"
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
	virtual void init(char* s);
	void out(Ostream& os)
		{ os << "Scanner"; }
	static Method<SuScanner>* methods()
		{
		static Method<SuScanner> methods[] =
			{
			Method<SuScanner>("Next", &SuScanner::Next),
			Method<SuScanner>("Position", &SuScanner::Position),
			Method<SuScanner>("Type", &SuScanner::Type),
			Method<SuScanner>("Text", &SuScanner::Text),
			Method<SuScanner>("Length", &SuScanner::Length),
			Method<SuScanner>("Value", &SuScanner::Valu),
			Method<SuScanner>("Keyword", &SuScanner::Keyword),
			Method<SuScanner>("Iter", &SuScanner::Iter),
			Method<SuScanner>("", 0)
			};
		return methods;
		}
	Value Next(BuiltinArgs&); // returns same as Text
	Value Position(BuiltinArgs&); // position after current token
	Value Type(BuiltinArgs&); // token number
	Value Text(BuiltinArgs&); // raw text 
	Value Length(BuiltinArgs&); // length of current token
	Value Valu(BuiltinArgs&); // for strings returns escaped
	Value Keyword(BuiltinArgs&); // keyword number (else zero)
	Value Iter(BuiltinArgs&);
protected:
	Scanner* scanner;
private:
	int token;
	};

Value su_scanner()
	{
	static BuiltinClass<SuScanner> clazz;
	return &clazz;
	}

template<>
Value BuiltinClass<SuScanner>::instantiate(BuiltinArgs& args)
	{
	args.usage("usage: Scanner(string)");
	char* s = args.getstr("string");
	args.end();
	SuScanner* scanner = new BuiltinInstance<SuScanner>();
	scanner->init(s);
	return scanner;
	}

void SuScanner::init(char* s)
	{
	scanner = new Scanner(s);
	}

Value SuScanner::Next(BuiltinArgs& args)
	{
	args.usage("usage: scanner.Next()");
	args.end();

	token = scanner->nextall();
	if (token == -1)
		return this;
	else
		return new SuString(scanner->source + scanner->prev, scanner->si - scanner->prev);
	}

Value SuScanner::Position(BuiltinArgs& args)
	{
	args.usage("usage: scanner.Position()");
	args.end();

	return scanner->si;
	}

Value SuScanner::Type(BuiltinArgs& args)
	{
	args.usage("usage: scanner.Type()");
	args.end();

	return token;
	}

Value SuScanner::Text(BuiltinArgs& args)
	{
	args.usage("usage: scanner.Text()");
	args.end();

	return new SuString(scanner->source + scanner->prev, scanner->si - scanner->prev);
	}

Value SuScanner::Length(BuiltinArgs& args)
	{
	args.usage("usage: scanner.Size()");
	args.end();

	return scanner->si - scanner->prev;
	}

Value SuScanner::Valu(BuiltinArgs& args)
	{
	args.usage("usage: scanner.Value()");
	args.end();

	return new SuString(scanner->value);
	}

Value SuScanner::Keyword(BuiltinArgs& args)
	{
	args.usage("usage: scanner.Keyword()");
	args.end();

	if (scanner->keyword && scanner->source[scanner->si] == ':')
		return 0;
	return scanner->keyword < KEYWORDS
		? scanner->keyword
		: scanner->keyword - KEYWORDS;
	}

Value SuScanner::Iter(BuiltinArgs& args)
	{
	args.usage("usage: scanner.Iter()");
	args.end();

	return this;
	}

// QueryScanner -----------------------------------------------------

class SuQueryScanner : public SuScanner
	{
public:
	virtual void init(char* s);
	static Method<SuQueryScanner>* methods()
		{
		return (Method<SuQueryScanner>*) SuScanner::methods();
		}
	};

Value su_queryscanner()
	{
	static BuiltinClass<SuQueryScanner> clazz;
	return &clazz;
	}

template<>
Value BuiltinClass<SuQueryScanner>::instantiate(BuiltinArgs& args)
	{
	args.usage("usage: QueryScanner(string)");
	char* s = args.getstr("string");
	args.end();
	SuQueryScanner* scanner = new BuiltinInstance<SuQueryScanner>();
	scanner->init(s);
	return scanner;
	}

#include "qscanner.h"

void SuQueryScanner::init(char* s)
	{
	scanner = new QueryScanner(s);
	}

