#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2002 Suneido Software Corp.
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
#include "mmfile.h"

class HistoryTable : public Query
	{
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
	void select(const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
	Row get(Dir dir) override;
	void set_transaction(int t) override;
	void close(Query* q) override
		{ }
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
