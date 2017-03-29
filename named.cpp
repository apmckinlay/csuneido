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

#include "named.h"
#include "symbols.h"
#include "globals.h"

gcstring Named::name() const
	{
	gcstring s;
	if (parent && parent->name() != "")
		s = s + parent->name() + parent->separator;
	if (num)
		s += (num < 0x8000 ? globals(num) : symstr(num));
	else if (str)
		s += str;
	return s;
	}

gcstring Named::library() const
	{
	if (lib != "")
		return lib;
	else if (parent)
		return parent->library();
	else
		return "";
	}

#include "ostreamstr.h"

// for debugging
gcstring Named::info() const
	{
	OstreamStr os;
	os << "Named {";
	if (lib != "")
		os << " lib: " << lib;
	if (parent)
		os << ", parent: " << parent->info();
	os << ", sep: '" << separator << "'";
	if (num)
		os << ", num: " << (num < 0x8000 ? globals(num) : symstr(num));
	else if (str)
		os << ", str: " << str;
	os << " }";
	return os.gcstr();
	}
