#ifndef QHISTABLE_H
#define QHISTABLE_H

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2002 Suneido Software Corp. 
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
#include "mmfile.h"

class HistoryTable : public Query
	{
public:
	explicit HistoryTable(const gcstring& table);
	void out(Ostream& os) const;
	Fields columns();
	Indexes indexes();
	Indexes keys();
	double optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze);
	// estimated sizes
	double nrecords();
	int recordsize();
	int columnsize();
	int keysize(const Fields& index);
	// iteration
	Header header();
	void select(const Fields& index, const Record& from, const Record& to);
	void rewind();
	Row get(Dir dir);
	void set_transaction(int t);
	void close(Query* q)
		{ }
private:
	Mmoffset next();
	Mmoffset prev();

	gcstring table;
	int tblnum;
	Mmfile::iterator iter;
	bool rewound;
	int id;
	int ic;
	};

#endif
