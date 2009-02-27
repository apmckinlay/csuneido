#ifndef QPROJECT_H
#define QPROJECT_H

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
	void out(Ostream& os) const;
	Query* transform();
	Fields columns()
		{ return flds; }
	Indexes keys();
	Indexes indexes();
	double optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze);
	Lisp<Fixed> fixed() const;
	// estimated result sizes
	double nrecords()
		{ return source->nrecords() / (strategy == COPY ? 1 : 2); }
	// iteration
	Header header();
	void select(const Fields& index, const Record& from, const Record& to);
	void rewind();
	Row get(Dir dir);

	bool updateable() const
		{ return Query1::updateable() && strategy == COPY; }
	bool output(const Record& r);
	bool update(const gcstring& index, const Record& key, const Record& rec);
	bool erase(const gcstring& index, const Record& key);
	void close(Query* q);
private:
	Fields flds;
	enum { NONE, COPY, SEQUENTIAL, LOOKUP } strategy;
	bool first;
	Header hdr;
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

	void ckmodify(char* action);
	};

#endif
