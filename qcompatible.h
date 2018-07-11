#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "queryimp.h"

class Compatible : public Query2
	{
public:
	Compatible(Query* s1, Query* s2);

	int recordsize() override
		{ return (source->recordsize() + source2->recordsize()) / 2; }
	int columnsize() override
		{ return (source->columnsize() + source2->columnsize()) / 2; }
protected:
	bool isdup(const Row& row);
	bool equal(const Row& r1, const Row& r2);

	Fields ki;
	Fields allcols;
	Header hdr1, hdr2;
	gcstring disjoint;
	friend class Project;
	};
