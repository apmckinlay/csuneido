#ifndef QEXTEND_H
#define QEXTEND_H

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
	Extend(Query* source, const Fields& f, Lisp<Expr*> e, const Fields& r);
	void init();
	void out(Ostream& os) const;
	Query* transform();
	Fields columns()
		{ return set_union(source->columns(), set_union(flds, rules)); }
	Indexes keys()
		{ return source->keys(); }
	Indexes indexes()
		{ return source->indexes(); }
	double optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze);
	// estimated result sizes
	int recordsize()
		{ return source->recordsize() + size(flds) * columnsize(); }
	// iteration
	Header header();
	Row get(Dir dir);
	void select(const Fields& index, const Record& from, const Record& to)
		{ source->select(index, from, to); }
	void rewind()
		{ source->rewind(); }
	Lisp<Fixed> fixed() const;

	bool output(const Record& r);
	// not private - accessed by Project::transform
	Fields flds;
	Lisp<Expr*> exprs;
	Fields eflds; // expr fields
	Fields rules;
private:
	void iterate_setup();

	bool first;
	Header hdr;
	Fields ats;
	short* srccolnums;
	mutable bool fixdone;
	mutable Lisp<Fixed> fix;
	};

#endif
