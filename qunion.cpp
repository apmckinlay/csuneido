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

#include "qunion.h"
#include <algorithm>

Query* Query::make_union(Query* s1, Query* s2)
	{
	return new Union(s1, s2);
	}

Union::Union(Query* s1, Query* s2) :
	Compatible(s1, s2), first(true), rewound(true), fixdone(false)
	{
	}

void Union::out(Ostream& os) const
	{
	os << "(" << *source << ") UNION";
	if (disjoint != "")
		os << "-DISJOINT (" << disjoint << ")";
	else
		os << (strategy == MERGE ? "-MERGE" : "-LOOKUP");
	if (! nil(ki))
		os << "^" << ki;
	os << " (" << *source2 << ") ";
	}

Indexes Union::indexes()
	{
	// TODO: there are more possible indexes
	return intersect(
		intersect(source->keys(), source->indexes()),
		intersect(source2->keys(), source2->indexes()));
	}

Indexes intersect_prefix(const Indexes keys1, const Indexes& keys2)
	{
	Indexes kout;
	for (Indexes k1 = keys1; ! nil(k1); ++k1)
		for (Indexes k2 = keys2; ! nil(k2); ++k2)
			if (prefix(*k1, *k2))
				kout.push(*k1);
			else if (prefix(*k2, *k1))
				kout.push(*k2);
	return lispset(kout);
	}

Indexes Union::keys()
	{
	if (disjoint != "")
		{
		Indexes kin = intersect_prefix(source->keys(), source2->keys());
		if (! nil(kin))
			{
			Indexes kout;
			for (; ! nil(kin); ++kin)
				if (member(*kin, disjoint))
					kout.push(*kin);
				else
					kout.push(concat(*kin, lisp(disjoint)));
			return kout;
			}
		}
	return Indexes(source->columns());
	}

const Fields none;

double Union::optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	Fields cols1 = source->columns();
	Fields needs1 = intersect(needs, cols1);
	Fields cols2 = source2->columns();
	Fields needs2 = intersect(needs, cols2);
	if (! nil(index))
		{
		// if not disjoint then index must also be a key
		if (disjoint == "" &&
			(! member(source->keys(), index) || ! member(source2->keys(), index)))
			return IMPOSSIBLE;
		if (freeze)
			{
			ki = index;
			strategy = MERGE;
			}
		return source->optimize(index, needs1, Fields(), is_cursor, freeze) +
			source2->optimize(index, needs2, Fields(), is_cursor, freeze);
		}
	else if (disjoint != "")
		{
		if (freeze)
			strategy = LOOKUP;
		return source->optimize(none, needs1, Fields(), is_cursor, freeze) +
			source2->optimize(none, needs2, Fields(), is_cursor, freeze);
		}
	else
		{
		Indexes k;
		// merge?
		Indexes keyidxs = intersect(
			intersect(source->keys(), source->indexes()),
			intersect(source2->keys(), source2->indexes()));
		Fields merge_key;
		double merge_cost = IMPOSSIBLE;
		for (k = keyidxs; ! nil(k); ++k)
			{
			// NOTE: optimize1 to avoid tempindex
			double cost = source->optimize1(*k, needs1, Fields(), is_cursor, false) +
				source2->optimize1(*k, needs2, Fields(), is_cursor, false);
			if (cost < merge_cost)
				{
				merge_key = *k;
				merge_cost = cost;
				}
			}
		// lookup on source2
		Fields ki2;
		double cost1 = IMPOSSIBLE;
		for (k = source2->keys(); ! nil(k); ++k)
			{
			Fields needs1_k = set_union(needs1, intersect(cols1, *k));
			double cost = 2 * source->optimize(none, needs1, needs1_k, is_cursor, false) +
				source2->optimize(*k, needs2, Fields(), is_cursor, false);
			if (cost < cost1)
				{
				ki2 = *k;
				cost1 = cost;
				}
			}
		// lookup on source1
		Fields ki1;
		double cost2 = IMPOSSIBLE;
		for (k = source->keys(); ! nil(k); ++k)
			{
			Fields needs2_k = set_union(needs2, intersect(cols2, *k));
			double cost = 2 * source2->optimize(none, needs2, needs2_k, is_cursor, false) +
				source->optimize(*k, needs1, Fields(), is_cursor, false) + OUT_OF_ORDER;
			if (cost < cost2)
				{
				ki1 = *k;
				cost2 = cost;
				}
			}

		double cost = min(merge_cost, min(cost1, cost2));
		if (cost >= IMPOSSIBLE)
			return IMPOSSIBLE;
		if (freeze)
			{
			if (merge_cost <= cost1 && merge_cost <= cost2)
				{
				strategy = MERGE;
				ki = merge_key;
				// NOTE: optimize1 to bypass tempindex
				source->optimize1(ki, needs1, Fields(), is_cursor, true);
				source2->optimize1(ki, needs2, Fields(), is_cursor, true);
				}
			else
				{
				strategy = LOOKUP;
				if (cost2 < cost1)
					{
					std::swap(source, source2);
					std::swap(needs1, needs2);
					std::swap(ki1, ki2);
					std::swap(cols1, cols2);
					}
				ki = ki2;
				Fields needs1_k = set_union(needs1, intersect(cols1, ki));
				// NOTE: optimize1 to bypass tempindex
				source->optimize1(none, needs1, needs1_k, is_cursor, true);
				source2->optimize(ki, needs2, Fields(), is_cursor, true);
				}
			}
		return cost;
		}
	}

Header Union::header()
	{
	return source->header() + source2->header();
	}

Lisp<Fixed> Union::fixed() const
	{
	if (fixdone)
		return fix;
	fixdone = true;
	Lisp<Fixed> fixed1 = source->fixed();
	Lisp<Fixed> fixed2 = source2->fixed();
	Lisp<Fixed> f1;
	Lisp<Fixed> f2;
	for (f1 = fixed1; ! nil(f1); ++f1)
		for (f2 = fixed2; ! nil(f2); ++f2)
			if (f1->field == f2->field)
				{
				fix.push(Fixed(f1->field, set_union(f1->values, f2->values)));
				break ;
				}
	Fields cols2 = source2->columns();
	for (f1 = fixed1; ! nil(f1); ++f1)
		if (! member(cols2, f1->field))
			fix.push(Fixed(f1->field, set_union(f1->values, lisp(SuEmptyString))));
	Fields cols1 = source->columns();
	for (f2 = fixed2; ! nil(f2); ++f2)
		if (! member(cols1, f2->field))
			fix.push(Fixed(f2->field, set_union(f2->values, lisp(SuEmptyString))));
	return fix;
	}

void Union::select(const Fields& index, const Record& from, const Record& to)
	{
	sel.org = from;
	sel.end = to;
	rewound = true;
	source->select(index, from, to);
	source2->select(index, from, to);
	}

void Union::rewind()
	{
	rewound = true;
	source->rewind();
	source2->select(ki, sel.org, sel.end);
	}

Row Union::get(Dir dir)
	{
	if (first)
		{
		// NOTE: first must be cleared by strategies
		verify(strategy == LOOKUP || strategy == MERGE);
		empty1 = Row(lispn(Record(), source->header().size()));
		empty2 = Row(lispn(Record(), source2->header().size()));
		}
	if (strategy == LOOKUP)
		{
		if (first)
			first = false;
		if (rewound)
			{
			rewound = false;
			in1 = (dir == NEXT);
			}
		Row row;
		for (;;) // need loop to go prev from src2 to src1
			{
			if (in1)
				{
				while (Eof != (row = source->get(dir)) && isdup(row))
					;
				if (row != Eof)
					return row + empty2;
				if (dir == PREV)
					return Eof;
				in1 = false;
				if (disjoint == "")
					source2->select(ki, sel.org, sel.end);
				}
			else // source2
				{
				row = source2->get(dir);
				if (row != Eof)
					return empty1 + row;
				if (dir == NEXT)
					return Eof;
				in1 = true;
				source->rewind();
				}
			}
		}
	else // strategy == MERGE
		{
		if (first)
			{
			first = false;
			hdr1 = source->header();
			hdr2 = source2->header();
			}

		// read from the appropriate source(s)
		if (rewound)
			{
			rewound = false;
			if (Eof != (row1 = source->get(dir)))
				key1 = row_to_key(hdr1, row1, ki);
			if (Eof != (row2 = source2->get(dir)))
				key2 = row_to_key(hdr2, row2, ki);
			}
		else
			{
			// curkey is required for changing direction
			if (src1 || before(dir, key1, 1, curkey, 2))
				{
				if (key1 == (dir == NEXT ? keymin : keymax))
					source->select(ki, sel.org, sel.end);
				row1 = source->get(dir);
				key1 = (row1 == Eof
					? (dir == NEXT ? keymax : keymin)
					: row_to_key(hdr1, row1, ki));
				}
			if (src2 || before(dir, key2, 2, curkey, 1))
				{
				if (key2 == (dir == NEXT ? keymin : keymax))
					source2->select(ki, sel.org, sel.end);
				row2 = source2->get(dir);
				key2 = (row2 == Eof
					? (dir == NEXT ? keymax : keymin)
					: row_to_key(hdr2, row2, ki));
				}
			}

		src1 = src2 = false;
		if (row1 == Eof && row2 == Eof)
			{
			curkey = key1; src1 = true;
			return Eof;
			}
		else if (row1 != Eof && row2 != Eof && equal(row1, row2))
			{
			// rows same so return either one
			curkey = key1; src1 = src2 = true;
			return row1 + empty2;
			}
		else if (row1 != Eof &&
			(row2 == Eof || before(dir, key1, 1, key2, 2)))
			{
			curkey = key1; src1 = true;
			return row1 + empty2;
			}
		else
			{
			curkey = key2; src2 = true;
			return empty1 + row2;
			}
		}
	}

// needed for ordering when key1 == key2 but row1 != row2
// NOTE: assumes row1 != row2 (this must be checked elsewhere)
bool Union::before(Dir dir, const Record& key1, int src1, const Record& key2, int src2)
	{
	if (key1 != key2)
		return dir == NEXT ? key1 < key2 : key1 > key2;
	else
		return dir == NEXT ? src1 < src2 : src1 > src2;
	}
