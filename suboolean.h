#ifndef SUBOOLEAN_H
#define SUBOOLEAN_H

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
	virtual int integer() const;
	virtual void out(Ostream& os);
	virtual gcstring gcstr() const;
	virtual SuNumber* number(); // defined by SuNumber & SuString

	virtual size_t packsize() const;
	virtual void pack(char* buf) const;
	static SuBoolean* unpack(const gcstring& s);

	static SuBoolean* literal(const char* s);

	virtual int order() const;
	virtual bool lt(const SuValue& x) const;
	virtual bool eq(const SuValue& x) const;

	static SuBoolean* t;
	static SuBoolean* f;
private:
	// private constructor to ensure only two instances
	explicit SuBoolean(int x) : val(x ? 1 : 0)
		{ }

	bool val;
	};

#endif
