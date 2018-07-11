// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "qproduct.h"
#include <algorithm>

Query* Query::make_product(Query* s1, Query* s2)
	{
	return new Product(s1, s2);
	}

Product::Product(Query* s1, Query* s2) : Query2(s1, s2), first(true)
	{
	Fields dups = intersect(s1->columns(), s2->columns());
	if (! nil(dups))
		except("product: common columns not allowed: " << dups);
	}

void Product::out(Ostream& os) const
	{
	os << "(" << *source << ") TIMES (" << *source2 << ")";
	}

Indexes Product::keys()
	{
	// keys are all pairs of source keys
	// there are no columns in common so no keys in common
	// so there won't be any duplicates in the result
	Indexes k;
	Indexes k1 = source->keys();
	for (; ! nil(k1); ++k1)
		{
		Indexes k2 = source2->keys();
		for (; ! nil(k2); ++k2)
			k.push(set_union(*k1, *k2));
		}
	verify(! nil(k));
	return k;
	}

const Fields none;

double Product::optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	Fields needs1 = intersect(source->columns(), needs);
	Fields needs2 = intersect(source2->columns(), needs);
	verify(size(set_union(needs1, needs2)) == size(needs));
	Fields firstneeds1 = intersect(source->columns(), needs);
	Fields firstneeds2 = intersect(source2->columns(), needs);
	if (! nil(firstneeds1) && ! nil(firstneeds2))
		firstneeds1 = firstneeds2 = Fields();

	double cost1 = source->optimize(index, needs1, firstneeds1, is_cursor, false) +
		source2->optimize(none, needs2, Fields(), is_cursor, false);
	double cost2 = source2->optimize(index, needs2, firstneeds2, is_cursor, false) +
		source->optimize(none, needs1, Fields(), is_cursor, false) + OUT_OF_ORDER;
	double cost = min(cost1, cost2);
	if (cost >= IMPOSSIBLE)
		return IMPOSSIBLE;
	if (! freeze)
		return cost;

	if (cost2 < cost1)
		{
		std::swap(source, source2);
		std::swap(needs1, needs2);
		std::swap(firstneeds1, firstneeds2);
		}
	source->optimize(index, needs1, firstneeds1, is_cursor, true);
	source2->optimize(none, needs2, Fields(), is_cursor, true);
	return cost;
	}

Header Product::header()
	{
	return source->header() + source2->header();
	}

Row Product::get(Dir dir)
	{
	Row row2 = source2->get(dir);
	if (first)
		{
		first = false;
		if (Eof == row2 || Eof == (row1 = source->get(dir)))
			return Eof;
		}
	if (row2 == Eof)
		{
		row1 = source->get(dir);
		if (row1 == Eof)
			return Eof;
		source2->rewind();
		row2 = source2->get(dir);
		}
	return row1 + row2;
	}

void Product::select(const Fields& index, const Record& from, const Record& to)
	{
	first = true;
	source->select(index, from, to);
	source2->rewind();
	}

void Product::rewind()
	{
	first = true;
	source->rewind();
	source2->rewind();
	}
