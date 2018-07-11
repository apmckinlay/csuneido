#pragma once
// Copyright (c) 2017 Suneido Software Corp. All rights reserved
// Licensed under GPLv2
#include "gcstring.h"

class CharMatcher
	{
public:
	virtual ~CharMatcher() = default;
	virtual bool matches(char ch) const;
	CharMatcher* negate();
	CharMatcher* or_(CharMatcher* cm);

	static CharMatcher NONE;
	static CharMatcher* is(char c);
	static CharMatcher* anyOf(const gcstring& chars);
	static CharMatcher* noneOf(const gcstring& chars);
	static CharMatcher* inRange(unsigned from, unsigned to);
protected:
	int indexIn(const gcstring& s, int start) const;
	};
