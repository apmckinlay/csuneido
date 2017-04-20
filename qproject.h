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
#include "index.h"

class Project : public Query1
	{
public:
	Project(Query* source, const Fields& f, bool allbut = false);
	void out(Ostream& os) const override;
	Query* transform() override;
	Fields columns() override
		{ return flds; }
	Indexes keys() override;
	Indexes indexes() override;
	double optimize2(const Fields& index, const Fields& needs, 
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	Lisp<Fixed> fixed() const override;
	// estimated result sizes
	double nrecords() override
		{ return source->nrecords() / (strategy == COPY ? 1 : 2); }
	// iteration
	Header header() override;
	void select(const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
	Row get(Dir dir) override;

	bool updateable() const override
		{ return Query1::updateable() && strategy == COPY; }
	bool output(const Record& r) override;
	void close(Query* q) override;
private:
	Fields flds;
	enum { NONE, COPY, SEQUENTIAL, LOOKUP } strategy;
	bool first;
	Header src_hdr;
	Header proj_hdr;
	// used by LOOKUP
	VVtree* idx;
	Keyrange sel;
	bool rewound;
	bool indexed;
	// used by SEQUENTIAL
	Row prevrow;
	Row currow;
	TempDest* td;
	Fields via;

	void includeDeps(const Fields& columns);
	void ckmodify(const char* action);
	static Fields withoutFixed(Fields fields, const Lisp<Fixed> fixed);
	static bool hasFixed(Fields fields, const Lisp<Fixed> fixed);
	};
