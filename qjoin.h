#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "queryimp.h"

class Join : public Query2 {
public:
	Join(Query* s1, Query* s2, Fields by);
	void out(Ostream& os) const override;
	Fields columns() override {
		return set_union(source->columns(), source2->columns());
	}
	Indexes keys() override;
	Indexes indexes() override;
	Fields ordering() override {
		return source->ordering();
	}
	Lisp<Fixed> fixed() const override;
	double optimize2(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	// estimated result sizes
	double nrecords() override;
	int recordsize() override;
	// iteration
	Header header() override;
	Row get(Dir dir) override;
	void select(
		const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
	enum Type { NONE, ONE_ONE, ONE_N, N_ONE, N_N };
	static Type reverse(Type type) {
		return type == ONE_N ? N_ONE : type == N_ONE ? ONE_N : type;
	}

	Fields joincols; // accessed by Project::transform
protected:
	Indexes keypairs() const;
	virtual const char* name() const {
		return "JOIN";
	}
	virtual bool can_swap() {
		return true;
	}
	virtual bool next_row1(Dir dir);
	virtual bool should_output(const Row& row);

	Type type;
	double nrecs;

private:
	double opt(Query* src1, Query* src2, Type typ, const Fields& index,
		const Fields& needs1, const Fields& needs2, bool is_cursor,
		bool freeze = false);

	bool first;
	Header hdr1;
	Row row1;
	Row row2;
	Row empty2;
};

class LeftJoin : public Join {
public:
	LeftJoin(Query* s1, Query* s2, Fields by);
	Indexes keys() override;
	double nrecords() override;

protected:
	const char* name() const override {
		return "LEFTJOIN";
	}
	bool can_swap() override {
		return false;
	}
	bool next_row1(Dir dir) override;
	bool should_output(const Row& row) override;

private:
	bool row1_output = false;
};
