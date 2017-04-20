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

#include "commalist.h"
#include "lisp.h"
#include "gcstring.h"

Lisp<gcstring> commas_to_list(const gcstring& s)
	{
	Lisp<gcstring> list;
	if (s.size() == 0)
		return list;
	int i, j;
	for (i = 0; -1 != (j = s.find(',', i)); i = j + 1)
		list.push(s.substr(i, j - i));
	list.push(s.substr(i));
	return list.reverse();
	}

gcstring list_to_commas(Lisp<gcstring> list)
	{
	gcstring s;
	for (; ! nil(list); ++list)
		{
		s += *list;
		if (! nil(cdr(list)))
			s += ",";
		}
	return s;
	}
