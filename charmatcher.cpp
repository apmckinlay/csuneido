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

#include "charmatcher.h"
#include "gcstring.h"

class CMIs : public CharMatcher
	{
public:
	CMIs(char c) : c(c)
		{ }
	bool matches(char ch) const override
		{
		return c == ch;
		}
private:
	const char c;
	};

class CMAnyOf : public CharMatcher
	{
public:
	CMAnyOf(const gcstring& chars) : chars(chars)
		{ }
	bool matches(char ch) const override
		{
		return chars.find(ch) != -1;
		}
private:
	const gcstring chars;
	};

class CMInRange : public CharMatcher
	{
public:
	CMInRange(unsigned from, unsigned to) : from(from), to(to)
		{ }
	bool matches(char ch) const override
		{
		unsigned c = ch;
		return from <= c && c <= to;
		}
private:
	const unsigned from;
	const unsigned to;
	};

class CMNegate : public CharMatcher
	{
public:
	CMNegate(CharMatcher* cm) : cm(cm)
		{ }
	bool matches(char ch) const override
		{
		return !cm->matches(ch);
		}
private:
	const CharMatcher* cm;
	};

class CMOr : public CharMatcher
	{
public:
	CMOr(CharMatcher* cm1, CharMatcher* cm2) : cm1(cm1), cm2(cm2)
		{ }
	bool matches(char ch) const override
		{
		return cm1->matches(ch) || cm2->matches(ch);
		}
private:
	const CharMatcher* cm1;
	const CharMatcher* cm2;
	};

bool CharMatcher::matches(char ch) const
	{
	return false;
	}

int CharMatcher::indexIn(const gcstring& s, int start) const
	{
	const char* p = s.begin() + start;
	int i = start;
	for (; p < s.end(); ++p, ++i)
		if (matches(*p))
			return i;
	return -1;
	}

CharMatcher CharMatcher::NONE;

CharMatcher* CharMatcher::anyOf(const gcstring& chars)
	{
	return new CMAnyOf(chars);
	}

CharMatcher* CharMatcher::noneOf(const gcstring& chars)
	{
	return (new CMAnyOf(chars))->negate();
	}

CharMatcher* CharMatcher::inRange(unsigned from, unsigned to)
	{
	return new CMInRange(from, to);
	}

CharMatcher* CharMatcher::is(char c)
	{
	return new CMIs(c);
	}

CharMatcher* CharMatcher::negate()
	{
	return new CMNegate(this);
	}

CharMatcher* CharMatcher::or_(CharMatcher* cm)
	{
	return new CMOr(this, cm);
	}

#include "testing.h"

class test_charMatcher : public Tests
	{
	TEST(0, main)
		{
		CMIs cmIs('a');
		assert_eq(cmIs.matches('a'), true);
		assert_eq(cmIs.matches('b'), false);

		CMAnyOf cmAnyOf("bcd");
		assert_eq(cmAnyOf.matches('b'), true);
		assert_eq(cmAnyOf.matches('e'), false);

		CMInRange cmInRange('e', 'g');
		assert_eq(cmInRange.matches('e'), true);
		assert_eq(cmInRange.matches('g'), true);
		assert_eq(cmInRange.matches('h'), false);

		CMNegate cmNegate(&cmInRange);
		assert_eq(cmNegate.matches('e'), false);
		assert_eq(cmNegate.matches('g'), false);
		assert_eq(cmNegate.matches('h'), true);

		CMOr cmOr(&cmAnyOf, &cmInRange);
		assert_eq(cmOr.matches('b'), true);
		assert_eq(cmOr.matches('e'), true);
		assert_eq(cmOr.matches('e'), true);
		assert_eq(cmOr.matches('g'), true);
		assert_eq(cmOr.matches('h'), false);

		assert_eq(CharMatcher::NONE.matches('a'), false);
		assert_eq(CharMatcher::NONE.matches('\n'), false);
		}
	};
REGISTER(test_charMatcher);
