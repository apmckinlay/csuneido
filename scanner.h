#pragma once
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

#include "opcodes.h"

class CodeVisitor;

// lexical scanner for Compiler
// QueryScanner is derived from this
class Scanner
	{
public:
	Scanner(const char* s = "", int i = 0, CodeVisitor* cv = nullptr);
	virtual ~Scanner() = default;
	int ahead() const;
	int aheadnl() const;
	virtual int next();
	int nextall();
	const char* rest() const
		{ return source + si; }
	static char doesc(const char* source, int& si);

	int prev = 0;
	const char* value = "";
	int len = 0;
	const char* err = "";
	enum { buflen = 20000 };		// maximum string constant length
	char buf[buflen];
	const char* source;
	int si;
	int keyword = 0;
	CodeVisitor* visitor = nullptr; // not used by Scanner
	// but placed here to avoid passing around extra argument in compiler
protected:
	explicit Scanner(const Scanner*);
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
	K_WHILE, K_DO, K_FOR, K_FOREVER, K_BREAK, K_CONTINUE,
	K_SWITCH, K_CASE, K_DEFAULT,
	K_FUNCTION, K_CLASS,
	K_CATCH,
	K_DLL, K_STRUCT, K_CALLBACK,
	K_NEW, K_RETURN, K_TRY, K_THROW, K_SUPER,
	K_TRUE, K_FALSE, K_IN, K_THIS,
	K_BOOL, K_INT8, K_INT16, K_INT32, K_INT64, K_POINTER,
	K_FLOAT, K_DOUBLE,
	K_HANDLE, K_GDIOBJ,
	K_STRING, K_BUFFER, K_RESOURCE, K_VOID,
	NEXT_KEYWORD
	};

const int Eof = -1;
