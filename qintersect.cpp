// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "qintersect.h"
#include <algorithm>

Query* Query::make_intersect(Query* s1, Query* s2)
	{
	return new Intersect(s1, s2);
	}

Intersect::Intersect(Query* s1, Query* s2) : Compatible(s1, s2)
	{
	}

void Intersect::out(Ostream& os) const
	{
	os << "(" << *source << ") INTERSECT";
	if (disjoint != "")
		os << "-DISJOINT";
	if (! nil(ki))
		os << "^" << ki;
	os << " (" << *source2 << ") ";
	}

double Intersect::optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	if (disjoint != "")
		return 0;
	Fields cols1 = source->columns();
	Fields needs1 = intersect(needs, cols1);
	Fields cols2 = source2->columns();
	Fields needs2 = intersect(needs, cols2);

	Fields ki2 = source2->key_index(needs2);
	Fields needs1_k = intersect(cols1, ki2);
	double cost1 = source->optimize(index, needs1, needs1_k, is_cursor, false) +
		source2->optimize(ki2, needs2, Fields(), is_cursor, false);

	Fields ki1 = source->key_index(needs1);
	Fields needs2_k = intersect(cols2, ki1);
	double cost2 = source2->optimize(index, needs2, needs2_k, is_cursor, false) +
		source->optimize(ki1, needs1, Fields(), is_cursor, false) + OUT_OF_ORDER;

	double cost = min(cost1, cost2);
	if (cost >= IMPOSSIBLE)
		return IMPOSSIBLE;
	if (freeze)
		{
		if (cost2 < cost1)
			{
			std::swap(source, source2);
			std::swap(needs1, needs2);
			std::swap(needs1_k, needs2_k);
			std::swap(ki1, ki2);
			}
		ki = ki2;
		source->optimize(index, needs1, needs1_k, is_cursor, true);
		source2->optimize(ki, needs2, Fields(), is_cursor, true);
		}
	return cost;
	}

Lisp<Fixed> Intersect::fixed() const
	{
	// TODO: should be union of fixed
	return source->fixed();
	}

Row Intersect::get(Dir dir)
	{
	if (disjoint != "")
		return Eof;
	Row row;
	while (Eof != (row = source->get(dir)) && ! isdup(row))
		;
	return row;
	}
