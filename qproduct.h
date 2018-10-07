#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "queryimp.h"

class Product : public Query2 {
public:
	Product(Query* s1, Query* s2);
	void out(Ostream& os) const override;
	Fields columns() override {
		return set_union(source->columns(), source2->columns());
	}
	Indexes indexes() override {
		return set_union(source->indexes(), source2->indexes());
	}
	Indexes keys() override;
	double optimize2(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	// estimated result sizes
	double nrecords() override {
		return source->nrecords() * source2->nrecords();
	}
	int recordsize() override {
		return source->recordsize() + source2->recordsize();
	}

	// iteration
	Header header() override;
	Row get(Dir dir) override;
	void select(const Fields& index, Record from, Record to) override;
	void rewind() override;

private:
	bool first;
	Row row1;
};
