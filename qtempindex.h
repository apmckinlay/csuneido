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

// sort a simple (single) source
struct TempIndex1 : public Query1
	{
	TempIndex1(Query* s, const Fields& o, bool u);
	void out(Ostream& os) const override;
	Indexes indexes() override;
	Fields columns() override
		{ return source->columns(); }
	Indexes keys() override
		{ return source->keys(); }
	// iteration
	Header header() override;
	void select(const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
	Row get(Dir dir) override;
	bool output(const Record& r) override
		{ return source->output(r); }
	void close(Query* q) override;
private:
	void iterate_setup(Dir dir);

	Fields order;
	bool unique;
	bool first;
	bool rewound;
	VFtree* index;
	VFtree::iterator iter;
	Keyrange sel;
	Header hdr;
	};

// sort a complex (multiple) source
struct TempIndexN : public Query1
	{
	TempIndexN(Query* s, const Fields& o, bool u);
	void out(Ostream& os) const override;
	Indexes indexes() override;
	Fields columns() override
		{ return source->columns(); }
	Indexes keys() override
		{ return source->keys(); }
	// iteration
	Header header() override
		{ return source->header(); }
	void select(const Fields& index, const Record& from, const Record& to) override;
	void rewind() override;
	Row get(Dir dir) override;
	bool output(const Record& r) override
		{ return source->output(r); }
	void close(Query* q) override;
private:
	void iterate_setup(Dir dir);

	Fields order;
	bool unique;
	bool first;
	bool rewound;
	VVtree* index;
	VVtree::iterator iter;
	Keyrange sel;
	Header hdr;
	};
