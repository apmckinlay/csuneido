#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2002 Suneido Software Corp.
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

#include "value.h"

class SuObject;

class SuSeq : public SuValue
	{
public:
	explicit SuSeq(Value iter);
	void out(Ostream& os) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;
	void putdata(Value i, Value x) override;
	Value getdata(Value) override;
	size_t packsize() const override;
	void pack(char* buf) const override;
	bool lt(const SuValue& y) const override;
	bool eq(const SuValue& y) const override;
	SuObject* ob_if_ob() override;
	SuObject* object() const;
private:
	void build() const;
	SuObject* copy() const;
	Value next() const;

	mutable Value iter;
	mutable SuObject* ob;
	};

class SuSeqIter : public SuValue
	{
public:
	SuSeqIter(Value from, Value to, Value by);
	void out(Ostream& os) const override;
	Value call(Value self, Value member, 
		short nargs, short nargnames, ushort* argnames, int each) override;
private:
	Value from;
	Value to;
	Value by;
	Value i;
	};
