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

#include "suboolean.h"
#include "sunumber.h"
#include "ostream.h"
#include "gcstring.h"

SuBoolean* SuBoolean::t = new SuBoolean(1);
SuBoolean* SuBoolean::f = new SuBoolean(0);

int SuBoolean::integer() const
	{ return val; }

SuNumber* SuBoolean::number()
	{
	return val ? &SuNumber::one : &SuNumber::zero;
	}

void SuBoolean::out(Ostream& os)
	{ os << gcstr(); }

gcstring SuBoolean::gcstr() const
	{
	static gcstring ts("true");
	static gcstring fs("false");
	return val ? ts : fs;
	}

size_t SuBoolean::packsize() const
	{ return 1; }

void SuBoolean::pack(char* buf) const
	{ *buf = val; }

SuBoolean* SuBoolean::unpack(const gcstring& s)
	{
	return s.size() > 0 && s[0] ? t : f;
	}

SuBoolean* SuBoolean::literal(const char* s)
	{
	if (0 == strcmp(s, "True") || 0 == strcmp(s, "true"))
		return t;
	else if (0 == strcmp(s, "False") || 0 == strcmp(s, "false"))
		return f;
	else
		return 0;
	}

static int ord = ::order("Boolean");

int SuBoolean::order() const
	{ return ord; }

bool SuBoolean::eq(const SuValue& y) const
	{
	if (y.order() == ord)
		return val == static_cast<const SuBoolean&>(y).val;
	else
		return false;
	}

bool SuBoolean::lt(const SuValue& y) const
	{
	if (y.order() == ord)
		return val < static_cast<const SuBoolean&>(y).val;
	else
		return ord < y.order();
	}
