#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "buffer.h"

class CodeVisitor;

// lexical scanner for Compiler
// QueryScanner is derived from this
class Scanner {
public:
	Scanner(const char* s = "", int i = 0, CodeVisitor* cv = nullptr);
	virtual ~Scanner() = default;
	int ahead() const;
	int aheadnl() const;
	virtual int next();
	int nextall();
	const char* rest() const {
		return source + si;
	}
	static char doesc(const char* source, int& si);

	int prev = 0;
	const char* value = "";
	int len = 0;
	const char* err = "";
	const char* source;
	int si;
	int keyword = 0;
	// visitor is not used by Scanner
	// but placed here to avoid passing around extra argument in compiler
	CodeVisitor* visitor = nullptr;
	Buffer buf;

protected:
	explicit Scanner(const Scanner*);
	virtual int keywords(const char*);
};

enum { // tokens
	T_ERROR = 1000,
	T_IDENTIFIER,
	T_NUMBER,
	T_STRING,
	T_AND,
	T_OR,
	T_WHITE,
	T_COMMENT,
	T_NEWLINE,
	T_RANGETO,
	T_RANGELEN
};

enum { // keywords
	KEYWORDS = 2000,
	K_IF,
	K_ELSE,
	K_WHILE,
	K_DO,
	K_FOR,
	K_FOREVER,
	K_BREAK,
	K_CONTINUE,
	K_SWITCH,
	K_CASE,
	K_DEFAULT,
	K_FUNCTION,
	K_CLASS,
	K_CATCH,
	K_DLL,
	K_STRUCT,
	K_CALLBACK,
	K_NEW,
	K_RETURN,
	K_TRY,
	K_THROW,
	K_SUPER,
	K_TRUE,
	K_FALSE,
	K_IN,
	K_THIS,
	K_BOOL,
	K_INT8,
	K_INT16,
	K_INT32,
	K_INT64,
	K_POINTER,
	K_FLOAT,
	K_DOUBLE,
	K_HANDLE,
	K_GDIOBJ,
	K_STRING,
	K_BUFFER,
	K_RESOURCE,
	K_VOID,
	NEXT_KEYWORD
};

const int Eof = -1;
