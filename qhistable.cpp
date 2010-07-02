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

#include "qhistable.h"
#include "database.h"
#include "thedb.h"
#include "recover.h"
#include "sudate.h"

Query* Query::make_history(const gcstring& table)
	{
	return new HistoryTable(table);
	}

HistoryTable::HistoryTable(const gcstring& t) : 
	table(t), tblnum(theDB()->ck_get_table(t)->num), rewound(true)
	{
	}

void HistoryTable::out(Ostream& os) const
	{
	os << "history(" << table << ")";
	}

Fields HistoryTable::columns()
	{
	Fields flds = theDB()->get_columns(table);
	flds.push("_action");
	flds.push("_date");
	return flds;
	}

Indexes HistoryTable::indexes()
	{
	return lisp(lisp(gcstring("_date")));
	}

Indexes HistoryTable::keys()
	{
	return lisp(lisp(gcstring("_date")));
	}

double HistoryTable::optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	return 100000;
	}

double HistoryTable::nrecords()
	{
	return 1000;
	}

int HistoryTable::recordsize()
	{
	return 100;
	}

int HistoryTable::columnsize()
	{
	return 10;
	}

int HistoryTable::keysize(const Fields& index)
	{
	return 100;
	}

Header HistoryTable::header()
	{
	return Header(
		lisp(
			Fields(),
			lisp(gcstring("_date"), gcstring("_action")),
			Fields(), 
			theDB()->get_fields(table)), 
		columns());
	}

void HistoryTable::select(const Fields& index, const Record& from, const Record& to)
	{
	rewound = true;
	}

void HistoryTable::rewind()
	{
	rewound = true;
	}

Mmoffset HistoryTable::next()
	{
	Commit* commit = 0;
	if (rewound)
		iter = theDB()->mmf->begin();
	else
		commit = (Commit*) *iter;
	rewound = false;
	const Mmfile::iterator end = theDB()->mmf->end();
	while (iter != end)
		{
		if (commit)
			{
			if (id + 1 < commit->ndeletes)
				return commit->deletes()[++id].unpack();
			id = commit->ndeletes;
			if (ic + 1 < commit->ncreates)
				return commit->creates()[++ic].unpack();
			ic = commit->ncreates;
			}
		do
			if (++iter == end)
				return 0;
			while (iter.type() != MM_COMMIT);
		commit = (Commit*) *iter;
		id = ic = -1;
		}
	return 0;
	}

Mmoffset HistoryTable::prev()
	{
	Commit* commit = 0;
	if (rewound)
		iter = theDB()->mmf->end();
	else
		commit = (Commit*) *iter;
	rewound = false;
	const Mmfile::iterator begin = theDB()->mmf->begin();
	while (iter != begin)
		{
		if (commit)
			{
			if (ic > 0)
				return commit->creates()[--ic].unpack();
			ic = -1;
			if (id > 0)
				return commit->deletes()[--id].unpack();
			id = -1;
			}
		do
			if (--iter == begin)
				return 0;
			while (iter.type() != MM_COMMIT);
		commit = (Commit*) *iter;
		id = commit->ndeletes;
		ic = commit->ncreates;
		}
	return 0;
	}

Row HistoryTable::get(Dir dir)
	{
	void* p;
	Mmoffset offset;
	do
		{
		offset = dir == NEXT ? next() : prev();
		if (offset == 0)
			return Eof;
		p = theDB()->mmf->adr(offset - sizeof (int));
		}
		while (*(int*) p != tblnum);
	Record r1;
	Commit* commit = (Commit*) *iter;
	r1.addval(SuDate::parse(ctime(&commit->t)));
	r1.addval(dir == NEXT 
		? 0 <= ic && ic < commit->ncreates ? "create" : "delete"
		: 0 <= id && id < commit->ndeletes ? "delete" : "create");
	Record r2(theDB()->mmf, offset);
	static Record emptyrec;
	return Row(lisp(emptyrec, r1, emptyrec, r2));
	}

void HistoryTable::set_transaction(int t)
	{
	}
