#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "queryimp.h"
#include "sustring.h"

class Summarize : public Query1
	{
public:
	Summarize(Query* source, const Fields& p, const Fields& c, const Fields& f, const Fields& o);
	void out(Ostream& os) const override;
	Fields columns() override;
	Indexes keys() override;
	Indexes indexes() override;
	double optimize2(const Fields& index, const Fields& needs, 
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	// estimated result sizes
	double nrecords() override;
	int recordsize() override
		{ return size(by) * source->columnsize() + size(cols) * 8; }
	Header header() override;
	void select(const Fields& index, const Record& from , const Record& to) override;
	void rewind() override;
	Row get(Dir dir) override;
	bool updateable() const override
		{ return false; }
	friend class Strategy;
	friend class SeqStrategy;
	friend class MapStrategy;
	friend class IdxStrategy;
private:
	bool by_contains_key() const;
	void iterate_setup();
	double idxCost(bool is_cursor, bool freeze);
	double seqCost(const Fields& index, const Fields& srcneeds,
		bool is_cursor, bool freeze);
	double mapCost(const Fields& index, const Fields& srcneeds,
		bool is_cursor, bool freeze);
	bool minmax1() const;
	Indexes sourceIndexes(const Fields& index) const;

	Fields by;
	Fields cols;
	Fields funcs;
	Fields on;
	enum { NONE, SEQ, MAP, IDX } strategy;
	bool first;
	bool rewound;
	Header hdr;
	Fields via;
	class Strategy* strategyImp{};
	bool wholeRecord;
	};
