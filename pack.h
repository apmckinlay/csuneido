#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// ReSharper disable once CppUnusedIncludeDirective
#include <cstddef> // for size_t

class Value;
class gcstring;

Value unpack(const char* buf, int len);
Value unpack(const gcstring& s);
gcstring unpack_gcstr(const gcstring& s);

size_t packsizeint(int n);
void packint(char* buf, int n);
int unpackint(const gcstring& s);

int packstrsize(const char* s);
int packstr(char* buf, const char* s);
const char* unpackstr(const char*& buf);

// in sort order
enum {
	PACK_FALSE,
	PACK_TRUE,
	PACK_MINUS,
	PACK_PLUS,
	PACK_STRING,
	PACK_DATE,
	PACK_OBJECT,
	PACK_RECORD
};
