#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2000 Suneido Software Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation - version 2.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License in the file COPYING
 * for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

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
	class Strategy* strategyImp;
	bool wholeRecord;
	};
