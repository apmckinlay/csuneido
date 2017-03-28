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

class Product : public Query2
	{
public:
	Product(Query* s1, Query* s2);
	void out(Ostream& os) const override;
	Fields columns() override
		{ return set_union(source->columns(), source2->columns()); }
	Indexes indexes() override
		{ return set_union(source->indexes(), source2->indexes()); }
	Indexes keys() override;
	double optimize2(const Fields& index, const Fields& needs, 
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	// estimated result sizes
	double nrecords() override
		{ return source->nrecords() * source2->nrecords(); }
	int recordsize() override
		{ return source->recordsize() + source2->recordsize(); }

	// iteration
	Header header() override;
	Row get(Dir dir) override;
	void select(const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
private:
	bool first;
	Row row1;
	};
