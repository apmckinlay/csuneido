#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "queryimp.h"

class Rename : public Query1 {
public:
	Rename(Query* source, const Fields& f, const Fields& t);
	void out(Ostream& os) const override;
	Query* transform() override;
	double optimize2(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	Fields columns() override {
		return rename_fields(source->columns(), from, to);
	}
	Indexes indexes() override {
		return rename_indexes(source->indexes(), from, to);
	}
	Indexes keys() override {
		return rename_indexes(source->keys(), from, to);
	}
	// iteration
	Header header() override;
	void select(const Fields& index, Record f, Record t) override {
		source->select(rename_fields(index, to, from), f, t);
	}
	void rewind() override {
		source->rewind();
	}
	Row get(Dir dir) override {
		return source->get(dir);
	}

	bool output(Record r) override {
		return source->output(r);
	}
	// private:
	static Fields rename_fields(Fields f, Fields from, Fields to);
	static Indexes rename_indexes(Indexes i, Fields from, Fields to);

	Fields from;
	Fields to;
};
