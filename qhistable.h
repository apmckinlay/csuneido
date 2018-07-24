#pragma once
// Copyright (c) 2002 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "queryimp.h"
#include "mmfile.h"

class HistoryTable : public Query {
public:
	explicit HistoryTable(const gcstring& table);
	void out(Ostream& os) const override;
	Fields columns() override;
	Indexes indexes() override;
	Indexes keys() override;
	double optimize2(const Fields& index, const Fields& needs,
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	// estimated sizes
	double nrecords() override;
	int recordsize() override;
	int columnsize() override;
	// iteration
	Header header() override;
	void select(
		const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
	Row get(Dir dir) override;
	void set_transaction(int t) override;
	void close(Query* q) override {
	}

private:
	Mmoffset next();
	Mmoffset prev();

	gcstring table;
	int tblnum;
	Mmfile::iterator iter;
	bool rewound = true;
	int id = 0;
	int ic = 0;
};
