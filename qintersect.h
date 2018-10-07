#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "qcompatible.h"

class Intersect : public Compatible {
public:
	Intersect(Query* s1, Query* s2);
	void out(Ostream& os) const override;
	Fields columns() override {
		return intersect(source->columns(), source2->columns());
	}
	Indexes keys() override {
		Indexes k = intersect(source->keys(), source2->keys());
		return nil(k) ? Indexes(columns()) : k;
	}
	Indexes indexes() override {
		return set_union(source->indexes(), source2->indexes());
	}
	double optimize2(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	Lisp<Fixed> fixed() const override;
	// estimated result sizes
	double nrecords() override {
		return min(source->nrecords(), source2->nrecords()) / 2;
	}
	// iteration
	Header header() override {
		return source->header();
	}
	void select(const Fields& index, Record from, Record to) override {
		source->select(index, from, to);
	}
	void rewind() override {
		source->rewind();
	}
	Row get(Dir dir) override;
};
