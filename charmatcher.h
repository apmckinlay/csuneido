#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
* This file is part of Suneido - The Integrated Application Platform
* see: http://www.suneido.com for more information.
*
* Copyright (c) 2017 Suneido Software Corp.
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
#include "gcstring.h"

class CharMatcher
	{
public:
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

class CMIs : public CharMatcher
	{
public:
	CMIs(char c) : c(c) 
		{ }
	bool matches(char ch) const override;
private:
	const char c;
	};

class CMAnyOf : public CharMatcher
	{
public:
	CMAnyOf(const gcstring& chars) : chars(chars) 
		{ }
	bool matches(char ch) const override;
private:
	const gcstring chars;
	};

class CMInRange : public CharMatcher
	{
public:
	CMInRange(unsigned from, unsigned to) : from(from), to(to)
		{ }
	bool matches(char ch) const override;
private:
	const unsigned from;
	const unsigned to;
	};

class CMNegate : public CharMatcher
	{
public:
	CMNegate(CharMatcher* cm) : cm(cm) 
		{ }
	bool matches(char ch) const override;
private:
	const CharMatcher* cm;
	};

class CMOr : public CharMatcher
	{
public:
	CMOr(CharMatcher* cm1, CharMatcher* cm2) : cm1(cm1), cm2(cm2)
		{ }
	bool matches(char ch) const override;
private:
	const CharMatcher* cm1;
	const CharMatcher* cm2;
	};