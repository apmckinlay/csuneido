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
#include "database.h"
#include "index.h"
#include "qselect.h"

class SuValue;

class Table : public Query
	{
public:
	explicit Table(const char* s);
	void out(Ostream& os) const override;
	Fields columns() override;
	Indexes indexes() override;
	Indexes keys() override;
	virtual float iselsize(const Fields& index, const Iselects& isels);
	double optimize2(const Fields& index, const Fields& needs, 
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	void select_index(const Fields& index);
	// estimated sizes
	double nrecords() override;
	int recordsize() override;
	int columnsize() override;
	virtual int keysize(const Fields& index); // overridden by select tests
	virtual int totalsize();
	virtual int indexsize(const Fields& index);
	// iteration
	Header header() override;
	void select(const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
	Row get(Dir dir) override;
	void set_transaction(int t) override
		{ tran = t; iter.set_transaction(t); }

	bool updateable() const override
		{ return true; }
	bool output(const Record& r) override;

	gcstring table;
	// used by Select for filters
	void set_index(const Fields& index);
	Index::iterator iter;

	void close(Query* q) override
		{ }
protected:
	Table() // used for tests
		{ }
	void iterate_setup(Dir dir);

	bool first = true;
	bool rewound = true;
	Keyrange sel;
	Header hdr;
	Index* idx = nullptr;
	int tran = INT_MAX;
	bool singleton = false; // i.e. key()
	Fields idxflds;
	Tbl* tbl = nullptr;
	};
