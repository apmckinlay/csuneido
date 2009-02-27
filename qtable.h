#ifndef QTABLE_H
#define QTABLE_H

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
#include "database.h"
#include "index.h"
#include "qselect.h"

class SuValue;

class Table : public Query
	{
public:
	explicit Table(char* s);
	void out(Ostream& os) const;
	Fields columns();
	Indexes indexes();
	Indexes keys();
	virtual float iselsize(const Fields& index, const Iselects& isels);
	double optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze);
	void select_index(const Fields& index);
	// estimated sizes
	double nrecords();
	int recordsize();
	int columnsize();
	int keysize(const Fields& index);
	virtual int totalsize();
	virtual int indexsize(const Fields& index);
	// iteration
	Header header();
	void select(const Fields& index, const Record& from, const Record& to);
	void rewind();
	Row get(Dir dir);
	void set_transaction(int t)
		{ tran = t; iter.set_transaction(t); }

	bool updateable() const
		{ return true; }
	bool output(const Record& r);
	bool erase(const gcstring& index, const Record& key);
	bool update(const gcstring& index, const Record& key, const Record& newrec);

	gcstring table;
	// used by Select for filters
	void set_index(const Fields& index);
	Index::iterator iter;

	void close(Query* q)
		{ }
protected:
	Table()
		{ }
	void iterate_setup(Dir dir);

	bool first;
	bool rewound;
	Keyrange sel;
	Header hdr;
	Index* ix;
	int tran;
	bool singleton; // i.e. key()
	Fields idx;
	Tbl* tbl;
	};

#endif
