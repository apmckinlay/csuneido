#ifndef QRENAME_H
#define QRENAME_H

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

class Rename : public Query1
	{
public:
	Rename(Query* source, const Fields& f, const Fields& t);
	void out(Ostream& os) const;
	Query* transform();
	double optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze);
	Fields columns()
		{ return rename_fields(source->columns(), from, to); }
	Indexes indexes()
		{ return rename_indexes(source->indexes(), from, to); }
	Indexes keys()
		{ return rename_indexes(source->keys(), from, to); }
	// iteration
	Header header();
	void select(const Fields& index, const Record& f, const Record& t)
		{ source->select(rename_fields(index, to, from), f, t); }
	void rewind()
		{ source->rewind(); }
	Row get(Dir dir)
		{ return source->get(dir); }

	bool output(const Record& r)
		{ return source->output(r); }
//private:
	static Fields rename_fields(Fields f, Fields from, Fields to);
	static Indexes rename_indexes(Indexes i, Fields from, Fields to);

	Fields from;
	Fields to;
	};

#endif
