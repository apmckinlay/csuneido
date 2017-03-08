#ifndef QDIFFERENCE_H
#define QDIFFERENCE_H

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

#include "qcompatible.h"

class Difference : public Compatible
	{
public:
	Difference(Query* s1, Query* s2);
	void out(Ostream& os) const;
	Fields columns()
		{ return source->columns(); }
	Indexes keys()
		{ return source->keys(); }
	Indexes indexes()
		{ return source->indexes(); }
	Query* transform();
	double optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze);
	Lisp<Fixed> fixed() const
		{ return source->fixed(); }
	// estimated result sizes
	double nrecords()
		{
		double n1 = source->nrecords();
		return (max(0.0, n1 - source2->nrecords()) + n1) / 2;
		}
	// iteration
	Header header()
		{ return source->header(); }
	void select(const Fields& index, const Record& from, const Record& to)
		{ source->select(index, from, to); }
	void rewind()
		{ source->rewind(); }
	Row get(Dir dir);
	};

#endif
