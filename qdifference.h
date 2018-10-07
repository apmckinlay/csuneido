#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "qcompatible.h"
#include <algorithm>
using std::max;

class Difference : public Compatible {
public:
	Difference(Query* s1, Query* s2);
	void out(Ostream& os) const override;
	Fields columns() override {
		return source->columns();
	}
	Indexes keys() override {
		return source->keys();
	}
	Indexes indexes() override {
		return source->indexes();
	}
	Query* transform() override;
	double optimize2(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	Lisp<Fixed> fixed() const override {
		return source->fixed();
	}

	double nrecords() override {
		double n1 = source->nrecords();
		return (max(0.0, n1 - source2->nrecords()) + n1) / 2;
	}

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
