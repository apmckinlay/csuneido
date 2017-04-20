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

class Expr;

class Extend : public Query1
	{
public:
	Extend(Query* source, const Fields& f, Lisp<Expr*> e);
	void init();
	void out(Ostream& os) const override;
	Query* transform() override;
	Fields columns() override
		{ return set_union(source->columns(), flds); }
	Indexes keys() override
		{ return source->keys(); }
	Indexes indexes() override
		{ return source->indexes(); }
	double optimize2(const Fields& index, const Fields& needs, 
		const Fields& firstneeds, bool is_cursor, bool freeze) override;
	// estimated result sizes
	int recordsize() override
		{ return source->recordsize() + size(flds) * columnsize(); }
	// iteration
	Header header() override;
	Row get(Dir dir) override;
	void select(const Fields& index, const Record& from, const Record& to) override
		{ source->select(index, from, to); }
	void rewind() override
		{ source->rewind(); }
	Lisp<Fixed> fixed() const override;
	bool has_rules();
	bool need_rule(Fields flds);

	bool output(const Record& r) override;
	// not private - accessed by Project::transform
	Fields flds;
	Lisp<Expr*> exprs;
	Fields eflds; // expr fields
private:
	void check_dependencies();
	void iterate_setup();
	Fields real_fields();
	bool need_rule(const gcstring& fld);

	bool first;
	Header hdr;
	Fields ats;
	mutable bool fixdone;
	mutable Lisp<Fixed> fix;
	};
