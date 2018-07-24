#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "qcompatible.h"

class Union : public Compatible {
public:
	Union(Query* s1, Query* s2);
	void out(Ostream& os) const override;
	Fields columns() override {
		return allcols;
	}
	Indexes indexes() override;
	Indexes keys() override;
	double optimize2(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	Lisp<Fixed> fixed() const override;
	// estimated result sizes
	double nrecords() override {
		return (source->nrecords() + source2->nrecords()) / 2;
	}
	// iteration
	Header header() override;
	void select(
		const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
	Row get(Dir dir) override;

private:
	enum { NONE, MERGE, LOOKUP } strategy = NONE;
	bool first = true;
	Row empty1;
	Row empty2;
	Keyrange sel;
	// for LOOKUP
	bool in1 = true; // true while processing first source
	// for MERGE
	static bool before(
		Dir dir, const Record& key1, int src1, const Record& key2, int src2);
	bool rewound = true;
	bool src1 = false;
	bool src2 = false;
	Row row1;
	Row row2;
	Record key1;
	Record key2;
	Record curkey;
	mutable bool fixdone = false;
	mutable Lisp<Fixed> fix;
};
