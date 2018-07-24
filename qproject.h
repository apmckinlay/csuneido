#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "queryimp.h"
#include "index.h"

class Project : public Query1 {
public:
	Project(Query* source, const Fields& f, bool allbut = false);
	void out(Ostream& os) const override;
	Query* transform() override;
	Fields columns() override {
		return flds;
	}
	Indexes keys() override;
	Indexes indexes() override;
	double optimize2(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	Lisp<Fixed> fixed() const override;
	// estimated result sizes
	double nrecords() override {
		return source->nrecords() / (strategy == COPY ? 1 : 2);
	}
	// iteration
	Header header() override;
	void select(
		const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
	Row get(Dir dir) override;

	bool updateable() const override {
		return Query1::updateable() && strategy == COPY;
	}
	bool output(const Record& r) override;
	void close(Query* q) override;

private:
	Fields flds;
	enum { NONE, COPY, SEQUENTIAL, LOOKUP } strategy = NONE;
	bool first = true;
	Header src_hdr;
	Header proj_hdr;
	// used by LOOKUP
	VVtree* idx{};
	Keyrange sel;
	bool rewound = true;
	bool indexed = false;
	// used by SEQUENTIAL
	Row prevrow;
	Row currow;
	TempDest* td{};
	Fields via;

	void includeDeps(const Fields& columns);
	void ckmodify(const char* action);
	static Fields withoutFixed(Fields fields, const Lisp<Fixed> fixed);
	static bool hasFixed(Fields fields, const Lisp<Fixed> fixed);
};
