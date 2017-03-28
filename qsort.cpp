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

#include "qsort.h"
#include "qscanner.h" // for DESCENDING

Query* Query::make_sort(Query* source, bool reverse, const Fields& segs)
	{
	return new QSort(source, reverse, segs);
	}

QSort::QSort(Query* source, bool r, const Fields& s) 
	: Query1(source), reverse(r), segs(s)
	{
	}

void QSort::out(Ostream& os) const
	{
	os << *source;
	if (! indexed)
		os << " SORT " << (reverse ? "REVERSE " : "") << segs;
	}

double QSort::optimize2(const Fields& index, const Fields& needs, 
	const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	verify(nil(index));
	// look for index containing requested index as prefix (using fixed)
	Fields best_index = segs;
	double best_cost = source->optimize(segs, needs, firstneeds, is_cursor, false);
	best_prefixed(source->indexes(), segs, needs, is_cursor, best_index, best_cost);

	if (! freeze)
		return best_cost;
	if (best_index == segs)
		return source->optimize(segs, needs, firstneeds, is_cursor, true);
	else
		// NOTE: optimize1 to avoid tempindex
		return source->optimize1(best_index, needs, firstneeds, is_cursor, true);
	}

Query* QSort::addindex()
	{
	indexed = true;
	return Query1::addindex();
	}
