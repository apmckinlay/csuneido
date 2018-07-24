#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "queryimp.h"
#include "index.h"

// sort a simple (single) source
struct TempIndex1 : public Query1 {
	TempIndex1(Query* s, const Fields& o, bool u);
	void out(Ostream& os) const override;
	Indexes indexes() override;
	Fields columns() override {
		return source->columns();
	}
	Indexes keys() override {
		return source->keys();
	}
	// iteration
	Header header() override;
	void select(
		const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
	Row get(Dir dir) override;
	bool output(const Record& r) override {
		return source->output(r);
	}
	void close(Query* q) override;

private:
	void iterate_setup(Dir dir);

	Fields order;
	bool unique;
	bool first;
	bool rewound;
	VFtree* index;
	VFtree::iterator iter;
	Keyrange sel;
	Header hdr;
};

// sort a complex (multiple) source
struct TempIndexN : public Query1 {
	TempIndexN(Query* s, const Fields& o, bool u);
	void out(Ostream& os) const override;
	Indexes indexes() override;
	Fields columns() override {
		return source->columns();
	}
	Indexes keys() override {
		return source->keys();
	}
	// iteration
	Header header() override {
		return source->header();
	}
	void select(
		const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
	Row get(Dir dir) override;
	bool output(const Record& r) override {
		return source->output(r);
	}
	void close(Query* q) override;

private:
	void iterate_setup(Dir dir);

	Fields order;
	bool unique;
	bool first;
	bool rewound;
	VVtree* index;
	VVtree::iterator iter;
	Keyrange sel;
	Header hdr;
};
