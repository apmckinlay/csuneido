#ifndef QSUMMARIZE_H
#define QSUMMARIZE_H
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
#include "sunumber.h"
#include "sustring.h"
#include "suobject.h" // for List

class Summarize : public Query1
	{
public:
	Summarize(Query* source, const Fields& p, const Fields& c, const Fields& f, const Fields& o);
	void out(Ostream& os) const;
	Fields columns()
		{ return set_union(by, cols); }
	Indexes keys();
	Indexes indexes();
	double optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze);
	// estimated result sizes
	double nrecords()
		{ return source->nrecords() / 2; }
	int recordsize()
		{
		return size(by) * source->columnsize() + size(cols) * 8;
		}
	Header header();
	void select(const Fields& index, const Record& from , const Record& to);
	void rewind();
	Row get(Dir dir);
	bool updateable() const
		{ return false; }
	friend class Strategy;
	friend class SeqStrategy;
	friend class MapStrategy;
private:
	bool by_contains_key();
	void iterate_setup();
	bool equal();

	Fields by;
	Fields cols;
	Fields funcs;
	Fields on;
	enum { NONE, COPY, SEQUENTIAL, MAP } strategy;
	bool first;
	bool rewound;
	Header hdr;
	Fields via;
	class Strategy* strategyImp;
	};

#endif
