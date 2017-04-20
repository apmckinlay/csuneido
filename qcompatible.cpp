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

#include "qcompatible.h"

Compatible::Compatible(Query* s1, Query* s2) :
	Query2(s1, s2), allcols(set_union(source->columns(), source2->columns()))
	{
	Lisp<Fixed> fixed1 = source->fixed();
	Lisp<Fixed> fixed2 = source2->fixed();
	Lisp<Fixed> f1;
	Lisp<Fixed> f2;
	for (f1 = fixed1; ! nil(f1); ++f1)
		for (f2 = fixed2; ! nil(f2); ++f2)
			if (f1->field == f2->field && nil(intersect(f1->values, f2->values)))
				{
				disjoint = f1->field;
				return ;
				}
	Fields cols2 = source2->columns();
	for (f1 = fixed1; ! nil(f1); ++f1)
		if (! member(cols2, f1->field) && ! member(f1->values, SuEmptyString))
			{
			disjoint = f1->field;
			return ;
			}
	Fields cols1 = source->columns();
	for (f2 = fixed2; ! nil(f2); ++f2)
		if (! member(cols1, f2->field) && ! member(f2->values, SuEmptyString))
			{
			disjoint = f2->field;
			return ;
			}
	}

bool Compatible::isdup(const Row& row)
	{
	if (disjoint != "")
		return false;

	// test if row is in source2
	if (nil(hdr1))
		{
		hdr1 = source->header();
		hdr2 = source2->header();
		}
	Record key = row_to_key(hdr1, row, ki);
	source2->select(ki, key);
	Row row2 = source2->get(NEXT);
	if (row2 == Eof)
		return false;
	return equal(row, row2);
	}

bool Compatible::equal(const Row& r1, const Row& r2)
	{
	if (disjoint != "")
		return false;

	for (Fields cols = allcols; ! nil(cols); ++cols)
		if (r1.getraw(hdr1, *cols) != r2.getraw(hdr2, *cols))
			return false;
	return true;
	}
