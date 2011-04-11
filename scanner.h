#ifndef SCANNER_H
#define SCANNER_H

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

#include "std.h"
#include "opcodes.h"

class CodeVisitor;

// lexical scanner for Compiler
// QueryScanner is derived from this
class Scanner
	{
public:
	explicit Scanner(char* = "", int = 0, CodeVisitor* cv = 0);
	char* peek();
	char peeknl();
	int next();
	int nextall();
	char* rest()
		{ return source + si; }

	int prev;
	char* value;
	int len;
	char* err;
	enum { buflen = 20000 };		// maximum string constant length
	char buf[buflen];
	char* source;
	int si;
	int keyword;
	CodeVisitor* visitor; // not used by Scanner
	// but placed here to avoid passing around extra argument in compiler
protected:
	char doesc(void);
	virtual int keywords(char*);
	};

enum // tokens
	{
	T_ERROR = 1000,
	T_IDENTIFIER, T_NUMBER, T_STRING,
	T_AND, T_OR,
	T_WHITE, T_COMMENT, T_NEWLINE,
	T_RANGETO, T_RANGELEN
	};

enum // keywords
	{
	KEYWORDS = 2000,
	K_IF, K_ELSE, 
	K_WHILE, K_DO, K_FOR, K_FOREACH, K_FOREVER, K_BREAK, K_CONTINUE,
	K_SWITCH, K_CASE, K_DEFAULT,
	K_FUNCTION, K_CLASS,
	K_CATCH,
	K_DLL, K_STRUCT, K_CALLBACK,
	K_NEW, K_RETURN, K_TRY, K_THROW, K_SUPER,
	K_TRUE, K_FALSE, K_VALUE, K_IN, K_LIST, K_THIS,
	K_BOOL, K_CHAR, K_SHORT, K_LONG, K_INT64, 
	K_FLOAT, K_DOUBLE, K_HANDLE, K_GDIOBJ,
	K_STRING, K_BUFFER, K_RESOURCE, K_VOID
	};

const int Eof = -1;

#endif
