#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "gcstring.h"

// specifies a part of a string matched by a part of a regular expression
struct Rxpart
	{
	const char* s;
	int n;
	const char* tmp; // used to store tentative "s"
	};

enum { MAXPARTS = 10 };

class gcstring;

// compile a regular expression
char* rx_compile(const gcstring& s);

// match a string against a compiled regular expression
bool rx_match(const char* s, int n, int i, const char* pat, Rxpart* psubs = 0);
inline bool rx_match(const gcstring& s, const char* pat, Rxpart* psubs = 0)
	{ return rx_match(s.ptr(), s.size(), 0, pat, psubs); }
bool rx_match_reverse(const char* s, int n, int i, const char* pat, Rxpart* psubs = 0);

// match a specific point in a string against a compiled regular expression
// returns -1 if no match, else the position past the match
int rx_amatch(const char* s, int i, int n, const char* pat, Rxpart* psubs = 0);

// determine the length of a replacement string
int rx_replen(const gcstring& rep, Rxpart* subs);

// build a replacement string
char* rx_mkrep(char* buf, const gcstring& rep, Rxpart* subs);
