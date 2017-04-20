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

#include "suvalue.h"

class gcstring;

// a boolean value (true or false)
class SuBoolean : public SuValue
	{
public:
	int integer() const override;
	void out(Ostream& os) const override;
	gcstring gcstr() const override;
	SuNumber* number() override; // defined by SuNumber & SuString

	size_t packsize() const override;
	void pack(char* buf) const override;
	static SuBoolean* unpack(const gcstring& s);

	static SuBoolean* literal(const char* s);

	int order() const override;
	bool lt(const SuValue& x) const override;
	bool eq(const SuValue& x) const override;

	static SuBoolean* t;
	static SuBoolean* f;
private:
	// private constructor to ensure only two instances
	explicit SuBoolean(int x) : val(x ? 1 : 0)
		{ }

	bool val;
	};
