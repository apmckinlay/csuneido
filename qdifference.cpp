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

#include "qdifference.h"

Query* Query::make_difference(Query* s1, Query* s2)
	{
	return new Difference(s1, s2);
	}

Difference::Difference(Query* s1, Query* s2) : Compatible(s1, s2)
	{
	}

void Difference::out(Ostream& os) const
	{
	os << "(" << *source << ") MINUS";
	if (disjoint != "")
		os << "-DISJOINT";
	if (! nil(ki))
		os << "^" << ki;
	os << " (" << *source2 << ") ";
	}

Query* Difference::transform()
	{
	// remove disjoint difference
	if (disjoint != "")
		return source->transform();
	else
		return Compatible::transform();
	}

double Difference::optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	if (disjoint != "")
		return 0;
	Fields cols1 = source->columns();
	Fields cols2 = source2->columns();
	Fields needs1 = intersect(needs, cols1);
	Fields needs2 = intersect(needs, cols2);
	ki = source2->key_index(needs2);
	Fields needs1_k = intersect(cols1, ki);
	return source->optimize(index, needs1, needs1_k, is_cursor, freeze) +
		source2->optimize(ki, needs2, Fields(), is_cursor, freeze);
	}

Row Difference::get(Dir dir)
	{
	Row row;
	while (Eof != (row = source->get(dir)) && isdup(row))
		;
	return row;
	}

