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

// make an online copy of the active database
// copy is made within a single transaction
// to ensure a consistent "snapshot"

// useful for online backup

// TODO: if no update transactions were started during the copy
// then you can switch over to the new database
// in order to "optimize" without shutting down

#include "dbcopy.h"
#include "database.h"
#include "gcstring.h"
#include "thedb.h"
#include "fibers.h" // for yield
#include <stdio.h> // for remove
#include <ctype.h> // for toupper
#include "sudate.h"
#include "fatal.h"

void compact()
	{
	char* tmp = tmpnam(NULL);
	if (*tmp == '\\')
		++tmp;
	db_copy(tmp);
	extern void close_db();
	close_db();
	remove("suneido.db.bak");
	if (0 != rename("suneido.db", "suneido.db.bak"))
		fatal("can't rename suneido.db to suneido.db.bak");
	if (0 != rename(tmp, "suneido.db"))
		fatal("can't rename temp file to suneido.db");
	}
	
struct DbCopy
	{
	DbCopy(char* dest);
	~DbCopy();

	void copy();
	void create_table(const gcstring& table);
	void copy_records(const gcstring& table);
	
	Database& thedb;
	Database newdb;
	int tran;
	};

void db_copy(char* dest)
	{
	remove(dest);
	DbCopy dbcopy(dest);
	dbcopy.copy();
	}

DbCopy::DbCopy(char* dest) : 
	thedb(*theDB()),
	newdb(dest, DBCREATE), 
	tran(thedb.transaction(READONLY))
	{
	Fibers::priority(-1);
	thedb.mmf->set_max_chunks_mapped(MM_MAX_CHUNKS_MAPPED / 2);
	newdb.mmf->set_max_chunks_mapped(MM_MAX_CHUNKS_MAPPED / 2);
	}

DbCopy::~DbCopy()
	{
	verify(thedb.commit(tran));
	Fibers::priority(0);
	}

void DbCopy::copy()
	{
	newdb.loading = true;
	
	// schema
	copy_records("views");
	Index::iterator iter;
	for (iter = thedb.get_index("tables", "tablename")->begin(schema_tran);
		! iter.eof(); ++iter)
		{
		Record r(iter.data());
		gcstring table = r.getstr(T_TABLE);
		if (! thedb.is_system_table(table))
			create_table(table);
		}
	
	// data
	for (iter = thedb.get_index("tables", "tablename")->begin(schema_tran);
		! iter.eof(); ++iter)
		{
		Record r(iter.data());
		gcstring table = r.getstr(T_TABLE);
		if (! thedb.is_system_table(table))
			copy_records(table);
		}
	}

void DbCopy::create_table(const gcstring& table)
	{
	newdb.add_table(table);
	Lisp<gcstring> f;
	// fields
	f = thedb.get_fields(table);
	for (; ! nil(f); ++f)
		if (*f != "-")
			newdb.add_column(table, *f);
	// rules
	for (f = thedb.get_rules(table); ! nil(f); ++f)
		{
		gcstring str(f->str()); // copy
		char* s = str.str();
		*s = toupper(*s);
		newdb.add_column(table, str);
		}
	// indexes
	Tbl* tbl = thedb.get_table(table);
	for (Lisp<Idx> ix = tbl->idxs; ! nil(ix); ++ix)
		newdb.add_index(table, ix->columns, ix->iskey, 
			ix->fksrc.table, ix->fksrc.columns, (Fkmode) ix->fksrc.mode,
			ix->index->is_unique());
	}

void DbCopy::copy_records(const gcstring& table)
	{
	Index* idx = thedb.first_index(table);
	if (! idx)
		except("table has no indexes: " << table);
	int i = 0;
	Tbl* newtbl = newdb.ck_get_table(table);
	Lisp<gcstring> fields = thedb.get_fields(table);
	static gcstring deleted = "-";
	bool squeeze = member(fields, deleted);
	int wtran = newdb.transaction(READWRITE);
	Mmoffset first = 0;
	Mmoffset last = 0;
	for (Index::iterator iter = idx->begin(tran); ! iter.eof(); ++iter)
		{
		Fibers::yieldif();
		Record rec(iter.data());
		if (squeeze)
			{
			Record newrec(rec.cursize());
			int j = 0;
			for (Lisp<gcstring> f = fields; ! nil(f); ++f, ++j)
				if (*f != "-")
					newrec.addraw(rec.getraw(j));
			rec = newrec;
			}
		else if (rec.size() > newtbl->nextfield)
			{
			Record newrec(rec.dup());
			newrec.truncate(newtbl->nextfield);
			rec = newrec;
			}
		if (table == "views")
			newdb.add_any_record(wtran, newtbl, rec);
		else
			{
			last = newdb.output_record(wtran, newtbl, rec);
			if (! first)
				first = last;
			}
		if (++i % 100 == 0)
			{
			verify(newdb.commit(wtran));
			wtran = newdb.transaction(READWRITE);
			}
		}
	verify(newdb.commit(wtran));
	if (table != "views")
		newdb.create_indexes(newtbl, first, last);
	}
