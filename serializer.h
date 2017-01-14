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

#include <cstdint>
#include "commalist.h"

class gcstring;
class Value;
class Buffer;

class Serializer
	{
public:
	Serializer(Buffer& r, Buffer& w);
	virtual ~Serializer() = default;

	void clear(); ///< Clears rdbuf and wrbuf
	Serializer& put(char c);
	Serializer& putBool(bool b);
	Serializer& putInt(int64_t n);
	Serializer& putStr(const char* s);
	Serializer& putStr(const gcstring& s);
	Serializer& putValue(Value value);
	Serializer& putInts(Lisp<int> list);
	Serializer& putStrings(Lisp<gcstring> list);

	char get();
	bool getBool();
	int64_t getInt();
	gcstring getStr();
	Value getValue();
	Lisp<int> getInts();
	Lisp<gcstring> getStrings();

protected:
	virtual void need(int n) = 0;
	virtual void read(char* buf, int n) = 0;
	Buffer& rdbuf;
	Buffer& wrbuf;
	};
