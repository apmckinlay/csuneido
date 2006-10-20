#ifndef QCOMPATIBLE_H
#define QCOMPATIBLE_H

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

class Compatible : public Query2
	{
public:
	Compatible(Query* s1, Query* s2);
	// estimated result sizes
	int recordsize()
		{ return (source->recordsize() + source2->recordsize()) / 2; }
	int columnsize()
		{ return (source->columnsize() + source2->columnsize()) / 2; }
protected:
	bool isdup(const Row& row);
	bool equal(const Row& r1, const Row& r2);

	Fields ki;
	Fields allcols;
	Header hdr1, hdr2;
	gcstring disjoint;
	friend class Project;
	};

#endif
