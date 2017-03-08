#ifndef QJOIN_H
#define QJOIN_H

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

class Join : public Query2
	{
public:
	Join(Query* s1, Query* s2, Fields by);
	void out(Ostream& os) const;
	Fields columns()
		{ return set_union(source->columns(), source2->columns()); }
	Indexes keys();
	Indexes indexes();
	Fields ordering()
		{ return source->ordering(); }
	Lisp<Fixed> fixed() const;
	double optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze);
	// estimated result sizes
	double nrecords();
	int recordsize();
	// iteration
	Header header();
	Row get(Dir dir);
	void select(const Fields& index, const Record& from, const Record& to);
	void rewind();
	enum Type { NONE, ONE_ONE, ONE_N, N_ONE, N_N };
	static Type reverse(Type type)
		{ return type == ONE_N ? N_ONE : type == N_ONE ? ONE_N : type; }

	Fields joincols; // accessed by Project::transform
protected:
	Indexes keypairs() const;
	virtual char* name() const
		{ return "JOIN"; }
	virtual bool can_swap()
		{ return true; }
	virtual bool next_row1(Dir dir);
	virtual bool should_output(const Row& row);

	Type type;
	double nrecs;
private:
	void getcols();
	double opt(Query* src1, Query* src2, Type typ,
		const Fields& index, const Fields& needs1, const Fields& needs2, bool is_cursor, bool freeze = false);

	bool first;
	Header hdr1;
	Row row1;
	Row row2;
	short* cols1;
	short* cols2;
	Row empty2;
	};

class LeftJoin : public Join
	{
public:
	LeftJoin(Query* s1, Query* s2, Fields by);
	Indexes keys();
	double nrecords();
protected:
	virtual char* name() const
		{ return "LEFTJOIN"; }
	virtual bool can_swap()
		{ return false; }
	virtual bool next_row1(Dir dir);
	virtual bool should_output(const Row& row);
private:
	bool row1_output;
	};

#endif
