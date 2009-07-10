#ifndef EXCEPTIMP_H
#define EXCEPTIMP_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2009 Suneido Software Corp. 
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

#include "sustring.h"

class SuObject;
class Frame;

class Except : public SuString
	{
public:
	explicit Except(gcstring s);
	Except(const Except* e, gcstring s);
	virtual Value call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each);
	SuObject* calls() const
		{ return calls_; }
	Frame* fp() const
		{ return fp_; }
	bool isBlockReturn() const
		{ return block_return; }
private:
	Frame* fp_;
	SuObject* calls_;
	bool block_return;
	};

#endif
