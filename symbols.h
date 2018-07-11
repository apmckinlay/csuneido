#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

class Value;
class SuValue;
class gcstring;

// return the symbol index for a char* string
short symnum(const char* s);

Value symbol_existing(const char* s);

// return the SuSymbol for a gcstring
Value symbol(const gcstring& s);

// return the SuSymbol for a char* string
Value symbol(const char* s);

// return the SuSymbol for a symbol index
Value symbol(int i);

// return the char* string for a symbol index
const char* symstr(int i);
