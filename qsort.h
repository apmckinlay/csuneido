#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "queryimp.h"

class QSort : public Query1 {
public:
	QSort(Query* source, bool r, const Fields& s);
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
	Header header() override {
		return source->header();
	}
	void select(const Fields& index, Record from, Record to) override {
		source->select(index, from, to);
	}
	void rewind() override {
		source->rewind();
	}
	Row get(Dir dir) override {
		return source->get(reverse ? (dir == NEXT ? PREV : NEXT) : dir);
	}
	double optimize2(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	Query* addindex() override;
	Fields ordering() override {
		return segs;
	}

	bool output(Record r) override {
		return source->output(r);
	}

	bool reverse;
	Fields segs;
	bool indexed = false;
};
