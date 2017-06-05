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

bool CMIs::matches(char ch) const
	{
	return c == ch;
	}

bool CMAnyOf::matches(char ch) const
	{
	return chars.find(ch) != -1;
	}

bool CMInRange::matches(char ch) const
	{
	unsigned c = ch;
	return from <= c && c <= to;
	}

bool CMNegate::matches(char ch) const
	{
	return !cm->matches(ch);
	}

bool CMOr::matches(char ch) const
	{
	return cm1->matches(ch) || cm2->matches(ch);
	}

bool CMNone::matches(char ch) const
	{
	return false;
	}

CharMatcher* CharMatcher::NONE = new CMNone();
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
		asserteq(cmIs.matches('a'), true);
		asserteq(cmIs.matches('b'), false);

		CMAnyOf cmAnyOf("bcd");
		asserteq(cmAnyOf.matches('b'), true);
		asserteq(cmAnyOf.matches('e'), false);

		CMInRange cmInRange('e', 'g');
		asserteq(cmInRange.matches('e'), true);
		asserteq(cmInRange.matches('g'), true);
		asserteq(cmInRange.matches('h'), false);

		CMNegate cmNegate(&cmInRange);
		asserteq(cmNegate.matches('e'), false);
		asserteq(cmNegate.matches('g'), false);
		asserteq(cmNegate.matches('h'), true);

		CMOr cmOr(&cmAnyOf, &cmInRange);
		asserteq(cmOr.matches('b'), true);
		asserteq(cmOr.matches('e'), true);
		asserteq(cmOr.matches('e'), true);
		asserteq(cmOr.matches('g'), true);
		asserteq(cmOr.matches('h'), false);

		CMNone cmNone;
		asserteq(cmNone.matches('a'), false);
		asserteq(cmNone.matches('\n'), false);
		}
	};
REGISTER(test_charMatcher);