// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "qsort.h"

Query* Query::make_sort(Query* source, bool reverse, const Fields& segs) {
	return new QSort(source, reverse, segs);
}

QSort::QSort(Query* source, bool r, const Fields& s)
	: Query1(source), reverse(r), segs(s) {
}

void QSort::out(Ostream& os) const {
	os << *source;
	if (!indexed)
		os << " SORT " << (reverse ? "REVERSE " : "") << segs;
}

double QSort::optimize2(const Fields& index, const Fields& needs,
	const Fields& firstneeds, bool is_cursor, bool freeze) {
	verify(nil(index));
	// look for index containing requested index as prefix (using fixed)
	Fields best_index = segs;
	double best_cost =
		source->optimize(segs, needs, firstneeds, is_cursor, false);
	best_prefixed(
		source->indexes(), segs, needs, is_cursor, best_index, best_cost);

	if (!freeze)
		return best_cost;
	if (best_index == segs)
		return source->optimize(segs, needs, firstneeds, is_cursor, true);
	else
		// NOTE: optimize1 to avoid tempindex
		return source->optimize1(
			best_index, needs, firstneeds, is_cursor, true);
}

Query* QSort::addindex() {
	indexed = true;
	return Query1::addindex();
}
