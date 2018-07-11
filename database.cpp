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

#include "database.h"
#include "port.h" // for fork_rebuild
#include "array.h"
#include "recover.h"
#include "commalist.h"
#include <ctype.h>
#ifdef _WIN32
#include <io.h> // for access
#include <process.h>
#else
#include <unistd.h> // for access
#endif
#include "fatal.h"
#include "checksum.h"
#include "value.h"
#include "fibers.h" // for yieldif for create_indexes

const int DB_VERSION = 1; // increment this for non-compatible database format changes

// Tbl ==============================================================

Tbl::Tbl(const Record& r, const Lisp<Col>& c, const Lisp<Idx>& i)
	: rec(r), cols(c), idxs(i), trigger(0)
	{
	num = rec.getlong(T_TBLNUM);
	name = rec.getstr(T_TABLE).to_heap();
	nextfield = rec.getlong(T_NEXTFIELD);
	nrecords = rec.getlong(T_NROWS);
	totalsize = rec.getlong(T_TOTALSIZE);
	}

void Tbl::update()
	{
	rec.reuse(T_NEXTFIELD);
	rec.addval(nextfield);
	rec.addval(nrecords);
	rec.addval(totalsize);
	}

// Idx ==============================================================

// TODO: combine the two copies of this
static Record key(const gcstring& table, const gcstring& columns)
	{
	Record r;
	r.addval(table);
	r.addval(columns);
	return r;
	}

Idx::Idx(const gcstring& table, const Record& r, const gcstring& c, short* n, 
	Index* i, Database* db)
	: index(i), nnodes(i->get_nnodes()), rec(r), columns(c), colnums(n),
		iskey(SuTrue == r.getval(I_KEY)), 
		fksrc(r.getstr(I_FKTABLE).to_heap(), r.getstr(I_FKCOLUMNS).to_heap(),
		      (Fkmode)r.getlong(I_FKMODE))
	{
	// find foreign keys pointing to this index
	for (Index::iterator iter = db->fkey_index->begin(schema_tran, key(table, columns));
		! iter.eof(); ++iter)
		{
		Record ri(iter.data());
		verify(ri.getstr(I_FKTABLE) == table);
		verify(ri.getstr(I_FKCOLUMNS) == columns);
		gcstring table2 = db->get_table(ri.getlong(I_TBLNUM))->name;
		fkdsts.push(Fkey(table2, ri.getstr(I_COLUMNS).to_heap(), 
			(Fkmode) ri.getlong(I_FKMODE)));
		}
	}

void Idx::update()
	{
	// assumes root & treelevels never change without nnodes changing
	if (index->get_nnodes() == nnodes)
		return ;
	nnodes = index->get_nnodes();
	rec.reuse(I_ROOT);
	index->getinfo(rec);
	}

// Tables ===========================================================

class Tables
	{
public:
	void add(Tbl* tbl)
		{
		bynum[tbl->num] = tbl;
		byname[tbl->name] = tbl;
		}
	Tbl* get(TblNum tblnum)
		{
		Tbl** t = bynum.find(tblnum);
		return t ? *t : 0;
		}
	Tbl* get(const gcstring& tblname)
		{
		Tbl** t = byname.find(tblname);
		return t ? *t : 0;
		}
	void erase(const gcstring& tblname)
		{
		if (! byname.find(tblname))
			return ;
		verify(bynum.erase(byname[tblname]->num));
		verify(byname.erase(tblname));
		}
	void erase(TblNum tblnum)
		{
		if (! bynum.find(tblnum))
			return ;
		gcstring tblname = bynum[tblnum]->name;
		verify(byname.erase(tblname));
		verify(bynum.erase(tblnum));
		}
private:
	HashMap<TblNum,Tbl*> bynum;
	HashMap<gcstring,Tbl*> byname;
	};

// Database ======================================================

Database::Database(const char* file, bool createmode)
	: loading(false),
	tables(new Tables), tables_index(0), columns_index(0), indexes_index(0),
	fkey_index(0), views_index(0), clock(1),
	cksum(::checksum(0,0,0)), output_type(MM_DATA)
	{
	bool existed = _access(file, 0) == 0;
	mmf = new Mmfile(file, createmode);
	if (existed && ! check_shutdown(mmf))
		{
		delete mmf; mmf = 0;
		if (0 != fork_rebuild())
			fatal("Database not rebuilt, unable to start");
		mmf = new Mmfile(file, createmode);
		verify(check_shutdown(mmf));
		}
	dest = new IndexDest(mmf);
	if (! mmf->first())
		{
		output_type = MM_OTHER;
		create();
		output_type = MM_DATA;
		}
	else
		{
		if (mmf->length(dbhdr()) < sizeof (Dbhdr) || dbhdr()->version != DB_VERSION)
			fatal("incompatible database\n\nplease dump with the old exe and load with the new one");
		new (adr(alloc(sizeof (Session), MM_SESSION))) Session(Session::STARTUP);
		mmf->sync();
		open();
		}
	}

Record ckroot(const Record& r)
	{
	verify(r.getmmoffset(I_ROOT));
	return r;
	}

void Database::open()
	{
	Mmoffset indexes = dbhdr()->indexes.unpack();
	indexes_index = mkindex(ckroot(input(indexes)));

	Record r = find(schema_tran, indexes_index, key(TN_INDEXES, "table,columns"));
	verify(! nil(r) && r.off() == indexes);

	tables_index = mkindex(ckroot(find(schema_tran, indexes_index, key(TN_TABLES, "tablename"))));
	tblnum_index = mkindex(ckroot(find(schema_tran, indexes_index, key(TN_TABLES, "table"))));
	columns_index = mkindex(ckroot(find(schema_tran, indexes_index, key(TN_COLUMNS, "table,column"))));
	fkey_index = mkindex(ckroot(find(schema_tran, indexes_index, key(TN_INDEXES, "fktable,fkcolumns"))));
	// WARNING: any new indexes added here must also be added in get_table
	}

Dbhdr* Database::dbhdr() const
	{
	return static_cast<Dbhdr*>(mmf->first());
	}

void Database::add_table(const gcstring& table)
	{
	if (istable(table))
		except("add table: table already exists: " << table);
	int tblnum = dbhdr()->next_table++;
	Record r = record(tblnum, table, 0, 0);
	add_any_record(schema_tran, "tables", r);
	table_create_act(tblnum);
	}

void Database::add_column(const gcstring& table, const gcstring& col)
	{
	if (col.has_suffix("_lower!"))
		return;
	Tbl* tbl = ck_get_table(table);

	int fldnum = isupper(col[0]) ? -1 : tbl->nextfield;
	if (col != "-") // addition of deleted field used by dump/load
		{
		gcstring column(col.uncapitalize());
		for (Lisp<Col> cols = tbl->cols; ! nil(cols); ++cols)
			if (cols->column == column)
				except("add column: column already exists: " << column << " in " << table);
		Record rec = record(tbl->num, column, fldnum);
		add_any_record(schema_tran, "columns", rec);
		}
	if (fldnum >= 0)
		{
		++tbl->nextfield;
		tbl->update();
		}
	tables->erase(table);
	}

void Database::add_index(const gcstring& table, const gcstring& columns, bool iskey,
	const gcstring& fktable, const gcstring& fkcolumns, Fkmode fkmode, bool unique)
	{
	for (auto cols = commas_to_list(columns); ! nil(cols); ++cols)
		if (cols->has_suffix("_lower!"))
			return;
	Tbl* tbl = ck_get_table(table);
	short* colnums = comma_to_nums(tbl->cols, columns);
	if (! colnums)
		except("add index: nonexistent column(s): " <<
			difference(commas_to_list(columns), get_columns(table)) << " in " << table);
	for (Lisp<Idx> idxs = tbl->idxs; ! nil(idxs); ++idxs)
		if (idxs->columns == columns)
			except("add index: index already exists: " << columns << " in " << table);
	Index* index = new Index(this, tbl->num, columns.str(), iskey, unique);

	if (! nil(tbl->idxs) && tbl->nrecords)
		{
		// insert existing records
		Index* idx = tbl->idxs->index; // use first index
		Tbl* fktbl = get_table(fktable);
		for (Index::iterator iter = idx->begin(schema_tran); ! iter.eof(); ++iter)
			{
			Record r(iter.data());
			if (fkey_source_block(schema_tran, fktbl, fkcolumns, project(r, colnums)))
				except("add index: blocked by foreign key: " << columns << " in " << table);
			Record key = project(r, colnums, iter->adr());
			if (! index->insert(schema_tran, Vslot(key)))
				except("add index: duplicate key: " << columns << " = " << key << " in " << table);
			}
		}

	Record r = record(tbl->num, columns,
		index, fktable, fkcolumns, fkmode);
	add_any_record(schema_tran, "indexes", r);
	tbl->idxs.append(Idx(table, r, columns, colnums, index, this));

	if (fktable != "")
		tables->erase(fktable); // update target
	}

bool Database::recover_index(Record& idxrec)
	{
	Tbl* tbl = get_table(idxrec.getlong(I_TBLNUM));
	if (! tbl)
		return false;
	gcstring columns = idxrec.getstr(I_COLUMNS);
	short* colnums = comma_to_nums(tbl->cols, columns);
	if (! colnums)
		return false; // invalid column name
	for (Lisp<Idx> idxs = tbl->idxs; ! nil(idxs); ++idxs)
		if (idxs->columns == columns)
			return false; // already exists
	bool iskey = idxrec.getval(I_KEY) == SuTrue;
	Index* index = new Index(this, tbl->num, columns.str(), iskey, false);

	if (! nil(tbl->idxs))
		{
		// insert existing records
		Index* idx = tbl->idxs->index; // use first index
		for (Index::iterator iter = idx->begin(schema_tran); ! iter.eof(); ++iter)
			{
			Record r(iter.data());
			Record key = project(r, colnums, iter->adr());
			if (! index->insert(schema_tran, Vslot(key)))
				return false;	// duplicate key
			}
		}
	idxrec.reuse(I_ROOT);
	index->getinfo(idxrec);
	add_index_entries(schema_tran, get_table(TN_INDEXES), idxrec);
	tbl->idxs.append(Idx(tbl->name, idxrec, columns, colnums, index, this));
	return true;
	}

void Database::invalidate_table(TblNum tblnum)
	{ tables->erase(tblnum); }

void Database::add_view(const gcstring& table, const gcstring& definition)
	{
	Record r;
	r.addval(table);
	r.addval(definition);
	add_any_record(schema_tran, "views", r);
	}

void Database::add_record(int tran, const gcstring& table, Record r)
	{
	if (is_system_table(table))
		except("add record: can't add records to system table: " << table);
	add_any_record(tran, table, r);
	}

Mmoffset Database::output(TblNum tblnum, Record& r)
	{
	int n = r.cursize();
	Mmoffset offset = alloc(sizeof (int) + n, output_type);
	void* p = adr(offset);
	*((int*) p) = tblnum;
	offset += sizeof (int);
	r.moveto(mmf, offset);
	// don't checksum tables or indexes records because they get updated
	if (output_type == MM_DATA && tblnum != TN_TABLES && tblnum != TN_INDEXES)
		checksum(p, sizeof (int) + n);
	return offset;
	}

void Database::add_any_record(int tran, Tbl* tbl, Record& r)
	{
	if (tran != schema_tran && ck_get_tran(tran)->type != READWRITE)
		except("can't output from read-only transaction to " << tbl->name);
	verify(tbl);
	verify(! nil(tbl->idxs));

	if (! loading)
		if (auto cols = fkey_source_block(tran, tbl, r))
			except("add record: blocked by foreign key: " << cols << " in " << tbl->name);

	if (tbl->num > TN_VIEWS && r.size() > tbl->nextfield)
		except("output: record has more fields (" << r.size() << ") than " << tbl->name << " should (" << tbl->nextfield << ")");
	Mmoffset off = output(tbl->num, r);
	try
		{
		add_index_entries(tran, tbl, r);
		}
	catch (...)
		{
		if (tran == schema_tran)
			delete_act(tran, tbl->num, off);
		throw ;
		}
	create_act(tran, tbl->num, off);

	if (! loading)
		{
		Record norec;
		tbl->user_trigger(tran, norec, r);
		}
	}

void Database::add_index_entries(int tran, Tbl* tbl, const Record& r)
	{
	Mmoffset off = r.off();
	for (Lisp<Idx> i = tbl->idxs; ! nil(i); ++i)
		{
		Record key = project(r, i->colnums, off);
		// handle insert failing due to duplicate key
		if (! i->index->insert(tran, Vslot(key)))
			{
			// delete from previous indexes
			for (Lisp<Idx> j = tbl->idxs; j->index != i->index; ++j)
				{
				Record key2 = project(r, j->colnums, off);
				verify(j->index->erase(key2));
				}
			except("duplicate key: " << i->columns << " = " << key << " in " << tbl->name);
			}
		i->update(); // update indexes record from index
		}

	++tbl->nrecords;
	tbl->totalsize += r.cursize();
	tbl->update(); // update tables record
	}

const char* Database::fkey_source_block(int tran, Tbl* tbl, const Record& rec)
	{
	for (Lisp<Idx> i = tbl->idxs; ! nil(i); ++i)
		if (i->fksrc.table != "" && fkey_source_block(tran,
			get_table(i->fksrc.table), i->fksrc.columns, project(rec, i->colnums)))
			return i->fksrc.columns.str();
	return 0;
	}

static bool key_empty(const Record& key)
	{
	for (int i = 0; i < key.size(); ++i)
		if (key.getraw(i).size() != 0)
			return false;
	return true;
	}

bool Database::fkey_source_block(int tran, Tbl* fktbl,
	const gcstring& fkcolumns, const Record& key)
	{
	if (fkcolumns == "" || key_empty(key))
		return false;
	Index* fkidx = get_index(fktbl, fkcolumns);
	if (! fkidx // other table doesn't exist or doesn't have index
		|| nil(find(tran, fkidx, key))) // other table doesn't have key
		return true;
	return false;
	}

// for use by dbcopy only - does NOT update indexes
Mmoffset Database::output_record(int tran, Tbl* tbl, Record& rec)
	{
	Mmoffset off = output(tbl->num, rec);
	++tbl->nrecords;
	tbl->totalsize += rec.cursize();
	tbl->update();
	create_act(tran, tbl->num, off);
	return off;
	}

// for use by dbcopy only
void Database::create_indexes(Tbl* tbl, Mmoffset first, Mmoffset last)
	{
	if (! tbl->nrecords)
		return ;
	// +- sizeof (int) is because int tblnum preceeds records
	first -= sizeof (int);
	Mmfile::iterator end(mmf->end());
	for (Lisp<Idx> ix = tbl->idxs; ! nil(ix); ++ix)
		{
		int n = 0;
		for (Mmfile::iterator iter(first, mmf); iter != end; ++iter)
			{
			if (iter.type() == MM_COMMIT)
				continue ;
			Fibers::yieldif();
			verify(iter.type() == MM_DATA);
			Mmoffset off = iter.offset() + sizeof (int);
			Record r(mmf, off);
			Record key = project(r, ix->colnums, off);
			if (! ix->index->insert(schema_tran, Vslot(key)))
				except("duplicate  " << ix->columns << " = " << key << " in " << tbl->name);
			++n;
			if (off == last)
				break ;
			}
		verify(n == tbl->nrecords);
		ix->update();
		}
	}

void Database::update_record(int tran, const gcstring& table, const gcstring& index,
	const Record& key, Record newrec)
	{
	update_any_record(tran, table, index, key, newrec);
	}

void Database::update_any_record(int tran, const gcstring& table, const gcstring& index,
	const Record& key, Record newrec)
	{
	Tbl* tbl = ck_get_table(table);
	Index* idx = get_index(table, index);
	verify(idx);
	Record oldrec = find(tran, idx, key);
	if (nil(oldrec))
		except("update record: can't find record in " << table);

	update_record(tran, tbl, oldrec, newrec);
	}

const bool NO_BLOCK = false;

Mmoffset Database::update_record(int tran, Tbl* tbl,
	const Record& oldrec, Record newrec, bool block)
	{
	if (tran != schema_tran)
		{
		if (ck_get_tran(tran)->type != READWRITE)
			except("can't update from read-only transaction in " << tbl->name);
		if (is_system_table(tbl->name))
			except("can't update records in system table: " << tbl->name);
		}

	if (tbl->num > TN_VIEWS && newrec.size() > tbl->nextfield)
		except("update: record has more fields (" << newrec.size() << ") than "
			<< tbl->name << " should (" << tbl->nextfield << ")");

	Mmoffset oldoff = oldrec.off();

	// check foreign keys
	Lisp<Idx> i;
	for (i = tbl->idxs; ! nil(i); ++i)
		{
		if (i->fksrc.table == "" && nil(i->fkdsts))
			continue ; // no foreign keys for this index
		Record oldkey = project(oldrec, i->colnums);
		Record newkey = project(newrec, i->colnums);
		if (oldkey == newkey)
			continue ;
		if ((block && fkey_source_block(tran, get_table(i->fksrc.table),
			i->fksrc.columns, newkey)) ||
			fkey_target_block(tran, *i, oldkey, newkey))
			except("update record in " << tbl->name << " blocked by foreign key");
		}

	if (! delete_act(tran, tbl->num, oldoff))
		{
		Transaction* t = ck_get_tran(tran);
		except("update record in " << tbl->name << " transaction conflict: " << t->conflict);
		}

	// do the update
	Record old = oldrec;
	Mmoffset newoff = output(tbl->num, newrec);

	// update indexes
	for (i = tbl->idxs; ! nil(i); ++i)
		{
		Record oldkey;
		if (tran == schema_tran)
			{
			oldkey = project(oldrec, i->colnums, oldoff);
			verify(i->index->erase(oldkey));
			}
		Record newkey = project(newrec, i->colnums, newoff);
		if (! i->index->insert(tran, Vslot(newkey)))
			{ // undo previous
			if (tran == schema_tran)
				i->index->insert(tran, Vslot(oldkey));
			for (Lisp<Idx> j = tbl->idxs; j != i; ++j)
				{
				Record newkey2 = project(newrec, j->colnums, newoff);
				verify(j->index->erase(newkey2));
				if (tran == schema_tran)
					{
					Record oldkey2 = project(oldrec, j->colnums, oldoff);
					verify(j->index->insert(tran, Vslot(oldkey2)));
					}
				}
			undo_delete_act(tran, tbl->num, oldoff);
			except("update record: duplicate key: " << i->columns << " = " << newkey << " in " << tbl->name);
			}
		i->update();
		}
	create_act(tran, tbl->num, newoff);
	tbl->totalsize += newrec.cursize() - oldrec.cursize();
	tbl->update();

	tbl->user_trigger(tran, old, newrec);
	return newoff;
	}

void Database::remove_table(const gcstring& table)
	{
	if (is_system_table(table))
		except("drop: can't destroy system table: " << table);
	remove_any_table(table);
	}

void Database::remove_any_table(const gcstring& table)
	{
	Tbl* tbl = ck_get_table(table);

	// remove indexes
	for (Lisp<Idx> p = tbl->idxs; ! nil(p); ++p)
		remove_any_index(tbl, p->columns);

	// remove columns
	for (Lisp<Col> q = tbl->cols; ! nil(q); ++q)
		remove_column(table, q->column);

	tables->erase(table);

	remove_any_record(schema_tran, "tables", "tablename", key(table));
	}

bool Database::is_system_table(const gcstring& table)
	{
	return
		table == "tables" ||
		table == "columns" ||
		table == "indexes" ||
		table == "views";
	}

bool Database::is_system_column(const gcstring& table, const gcstring& column)
	{
	return
		(table == "tables" &&
			(column == "table" ||
			column == "nrows" ||
			column == "totalsize")) ||
		(table == "columns" &&
			(column == "table" ||
			column == "column" ||
			column == "field")) ||
		(table == "indexes" &&
			(column == "table" ||
			column == "columns" ||
			column == "root" ||
			column == "treelevels" ||
			column == "nnodes"));
	}

bool Database::is_system_index(const gcstring& table, const gcstring& columns)
	{
	return(table == "tables" && columns == "table") ||
		(table == "columns" && columns == "table,column") ||
		(table == "indexes" && columns == "table,columns");
	}

void Database::remove_column(const gcstring& table, const gcstring& column)
	{
	if (is_system_column(table, column))
		except("delete column: can't delete system column: " <<
			column << " from " << table);

	Tbl* tbl = ck_get_table(table);

	// ensure column not used in index
	gcstring col = "," + column + ",";
	for (Lisp<Idx> p = tbl->idxs; ! nil(p); ++p)
		{
		gcstring cols = "," + p->columns + ",";
		if (-1 != cols.find(col))
			except("delete column: can't delete column used in index: " << column << " in " << table);
		}

	// remove column from tbl->cols
	Lisp<Col> q;
	for (q = tbl->cols; ! nil(q); ++q)
		if (q->column == column)
			break ;
	if (nil(q))
		except("delete column: nonexistent column: " << column << " in " << table);
	tbl->cols.erase(*q);

	remove_any_record(schema_tran, "columns", "table,column", key(tbl->num, column));
	}

void Database::remove_index(const gcstring& table, const gcstring& columns)
	{
	if (is_system_index(table, columns))
		except("delete index: can't delete system index: " <<
			columns << " from " << table);
	Tbl* tbl = ck_get_table(table);
	if (nil(cdr(tbl->idxs)))
		except("delete index: can't delete last index from " << table);
	remove_any_index(tbl, columns);
	}

void Database::remove_any_index(Tbl* tbl, const gcstring& columns)
	{
	verify(tbl);

	// remove from tbl->idxs
	Lisp<Idx> p = tbl->idxs;
	for (; ! nil(p); ++p)
		if (p->columns == columns)
			break ;
	if (nil(p))
		except("delete index: nonexistent index: " << columns << " in " << tbl->name);
	tbl->idxs.erase(*p);

	remove_any_record(schema_tran, "indexes", "table,columns", key(tbl->num, columns));
	}

void Database::remove_record(int tran, const gcstring& table, const gcstring& index, const Record& key)
	{
	remove_any_record(tran, table, index, key);
	}

Record Database::find(int tran, Index* index, const Record& key)
	{
	if (! index)
		return Record();
	Vslot vs = index->find(tran, key);
	return nil(vs) ? Record() : input(vs.adr());
	}

void Database::remove_any_record(int tran, const gcstring& table, const gcstring& index, const Record& key)
	{
	Tbl* tbl = ck_get_table(table);
	// lookup key in given index
	Index* idx = get_index(table, index);
	verify(idx);
	Record rec(find(tran, idx, key));
	if (nil(rec))
		except("delete record: can't find record in " << table);
	remove_record(tran, tbl, rec);
	}

void Database::remove_record(int tran, Tbl* tbl, const Record& r)
	{
	if (tran != schema_tran)
		{
		if (ck_get_tran(tran)->type != READWRITE)
			except("can't delete from read-only transaction in " << tbl->name);
		if (is_system_table(tbl->name))
			except("delete record: can't delete records from system table: " << tbl->name);
		}
	verify(tbl);
	verify(! nil(r));

	if (auto fktblname = fkey_target_block(tran, tbl, r))
		except("delete record from " << tbl->name << " blocked by foreign key from " << fktblname);

	if (! delete_act(tran, tbl->num, r.off()))
		{
		Transaction* t = ck_get_tran(tran);
		except("delete record from " << tbl->name << " transaction conflict: " << t->conflict);
		}

	if (tran == schema_tran)
		remove_index_entries(tbl, r);

	--tbl->nrecords;
	tbl->totalsize -= r.cursize();
	tbl->update(); // update tables record

	if (! loading)
		{
		Record norec;
		tbl->user_trigger(tran, r, norec);
		}
	}

void Database::remove_index_entries(Tbl* tbl, const Record& r)
	{
	Mmoffset off = r.off();
	for (Lisp<Idx> i = tbl->idxs; ! nil(i); ++i)
		{
		Record key = project(r, i->colnums, off);
		verify(i->index->erase(key));
		i->update(); // update indexes record from index
		}
	}

const char* Database::fkey_target_block(int tran, Tbl* tbl, const Record& r)
	{
	for (Lisp<Idx> i = tbl->idxs; ! nil(i); ++i)
		if (auto fktblname = fkey_target_block(tran, *i, project(r, i->colnums)))
			return fktblname;
	return 0;
	}

const char* Database::fkey_target_block(
	int tran, const Idx& idx, const Record& key, const Record newkey)
	{
	if (key_empty(key))
		return 0;
	for (Lisp<Fkey> fk = idx.fkdsts;  ! nil(fk); ++fk)
		{
		Tbl* fktbl = get_table(fk->table);
		Index* fkidx = get_index(fktbl, fk->columns);
		if (! fkidx)
			continue ;
		Index::iterator iter = fkidx->begin(tran, key);
		if (nil(newkey) && (fk->mode & CASCADE_DELETES))
			{
			for (; ! iter.eof(); ++iter)
				{
				Record r(iter.data());
				remove_record(tran, fktbl, r);
				iter.reset_prevsize();
				// need to reset prevsize in case trigger updates other lines
				// otherwise iter doesn't "see" the updated lines
				}
			}
		else if (! nil(newkey) && (fk->mode & CASCADE_UPDATES))
			{
			Lisp<Idx> fi;
			for (fi = fktbl->idxs; ! nil(fi); ++fi)
				if (fi->columns == fk->columns)
					break ;
			verify(! nil(fi));
			short* colnums = fi->colnums;
			for (; ! iter.eof(); ++iter)
				{
				Record oldrec(iter.data());
				Record newrec;
				for (int i = 0; i < oldrec.size(); ++i)
					{
					int j = 0;
					for (; colnums[j] != END && colnums[j] != i; ++j)
						;
					if (colnums[j] == END)
						newrec.addraw(oldrec.getraw(i));
					else
						newrec.addraw(newkey.getraw(j));
					}
				update_record(tran, fktbl, oldrec, newrec, NO_BLOCK);
				}
			}
		else // blocking
			{
			// can't use find() because other is index not key
			if (! iter.eof())
				return fktbl->name.str();
			}
		}
	return 0;
	}

Lisp< Lisp<gcstring> > Database::get_indexes(const gcstring& table)
	{
	Lisp< Lisp<gcstring> > list;
	Tbl* tbl = get_table(table);
	if (! tbl)
		return list;
	for (Lisp<Idx> idxs = tbl->idxs; ! nil(idxs); ++idxs)
		list.push(commas_to_list(idxs->columns));
	return list.reverse();
	}

Lisp< Lisp<gcstring> > Database::get_keys(const gcstring& table)
	{
	Lisp< Lisp<gcstring> > list;
	Tbl* tbl = get_table(table);
	if (! tbl)
		return list;
	for (Lisp<Idx> idxs = tbl->idxs; ! nil(idxs); ++idxs)
		if (idxs->iskey)
			list.push(commas_to_list(idxs->columns));
	return list.reverse();
	}

Lisp< Lisp<gcstring> > Database::get_nonkeys(const gcstring& table)
	{
	Lisp< Lisp<gcstring> > list;
	Tbl* tbl = get_table(table);
	if (! tbl)
		return list;
	for (Lisp<Idx> idxs = tbl->idxs; ! nil(idxs); ++idxs)
		if (! idxs->iskey)
			list.push(commas_to_list(idxs->columns));
	return list.reverse();
	}

// logical columns i.e. includes rules but not deleted fields
Lisp<gcstring> Database::get_columns(const gcstring& table)
	{
	Lisp<gcstring> list;
	Tbl* tbl = get_table(table);
	if (! tbl)
		return list;
	for (Lisp<Col> cols = tbl->cols; ! nil(cols); ++cols)
		list.push(cols->column);
	return list.reverse();
	}

// physical fields i.e. includes deleted but not rules (1:1 match with records)
Lisp<gcstring> Database::get_fields(const gcstring& table)
	{
	Tbl* tbl = get_table(table);
	return tbl ? tbl->get_fields() : Lisp<gcstring>();
	}

Lisp<gcstring> Tbl::get_fields()
	{
	Lisp<gcstring> list;
	int i = 0;
	for (Lisp<Col> cs = cols; ! nil(cs); ++cs)
		{
		if (cs->colnum < 0)
			continue ; // skip rules
		for (; i < cs->colnum; ++i)
			list.push("-");
		list.push(cs->column);
		++i;
		}
	for (; i < nextfield; ++i)
		list.push("-");
	return list.reverse();
	}

Lisp<gcstring> Database::get_rules(const gcstring& table)
	{
	Lisp<gcstring> list;
	Tbl* tbl = get_table(table);
	if (! tbl)
		return list;
	for (Lisp<Col> cols = tbl->cols; ! nil(cols); ++cols)
		if (cols->colnum < 0)
			list.push(cols->column);
	return list.reverse();
	}

// create ===========================================================

// requires a little trickiness to bootstrap the system tables
void Database::create()
	{
	loading = true;

	Dbhdr* hdr = static_cast<Dbhdr*>(adr(alloc(sizeof (Dbhdr), MM_OTHER)));
	verify(dbhdr() == hdr);
	hdr->version = DB_VERSION;

	// tables
	tables_index = new Index(this, TN_TABLES, "tablename", true);
	tblnum_index = new Index(this, TN_TABLES, "table", true);
	table_record(TN_TABLES, "tables", 3, 5);
	table_record(TN_COLUMNS, "columns", 17, 3);
	table_record(TN_INDEXES, "indexes", 5, 9);
	hdr->next_table = TN_INDEXES + 1;

	// columns
	columns_index = new Index(this, TN_COLUMNS, "table,column", true);
	columns_record(TN_TABLES, "table", 0);
	columns_record(TN_TABLES, "tablename", 1);
	columns_record(TN_TABLES, "nextfield", 2);
	columns_record(TN_TABLES, "nrows", 3);
	columns_record(TN_TABLES, "totalsize", 4);

	columns_record(TN_COLUMNS, "table", 0);
	columns_record(TN_COLUMNS, "column", 1);
	columns_record(TN_COLUMNS, "field", 2);

	columns_record(TN_INDEXES, "table", 0);
	columns_record(TN_INDEXES, "columns", 1);
	columns_record(TN_INDEXES, "key", 2);
	columns_record(TN_INDEXES, "fktable", 3);
	columns_record(TN_INDEXES, "fkcolumns", 4);
	columns_record(TN_INDEXES, "fkmode", 5);
	columns_record(TN_INDEXES, "root", 6);
	columns_record(TN_INDEXES, "treelevels", 7);
	columns_record(TN_INDEXES, "nnodes", 8);

	// indexes
	indexes_index = new Index(this, TN_INDEXES, "table,columns", true);
	fkey_index = new Index(this, TN_INDEXES, "fktable,fkcolumns", false);
	indexes_record(tables_index);
	indexes_record(tblnum_index);
	indexes_record(columns_index);
	// output indexes indexes last
	hdr->indexes = indexes_record(indexes_index);
	indexes_record(fkey_index);

	// views
	add_table("views");
	add_column("views", "view_name");
	add_column("views", "view_definition");
	add_index("views", "view_name", true);
	}

void Database::table_record(TblNum tblnum, const char* tblname, int nrows, int nextfield)
	{
	Record r = record(tblnum, tblname, nrows, nextfield, 100);
	Mmoffset at = output(TN_TABLES, r);
	Record key1;
	key1.addval(tblnum);
	key1.addmmoffset(at);
	verify(tblnum_index->insert(schema_tran, Vslot(key1)));
	Record key2;
	key2.addval(tblname);
	key2.addmmoffset(at);
	verify(tables_index->insert(schema_tran, Vslot(key2)));
	}

void Database::columns_record(TblNum tblnum, const char* column, int field)
	{
	Record r;
	r.addval(tblnum);
	r.addval(column);
	r.addval(field);
	Mmoffset at = output(TN_COLUMNS, r);
	Record key;
	key.addval(tblnum);
	key.addval(column);
	key.addmmoffset(at);
	verify(columns_index->insert(schema_tran, Vslot(key)));
	}

Mmoffset Database::indexes_record(Index* index)
	{
	Record r = record(index->tblnum, index->idxname, index, "", "", BLOCK);
	Mmoffset at = output(TN_INDEXES, r);
	Record key1;
	key1.addval(index->tblnum);
	key1.addval(index->idxname);
	key1.addmmoffset(at);
	verify(indexes_index->insert(schema_tran, Vslot(key1)));
	Record key2;
	key2.addval(""); // fktable
	key2.addval(""); // fkcolumns
	key2.addmmoffset(at);
	verify(fkey_index->insert(schema_tran, Vslot(key2)));
	return at;
	}

// tables records
Record Database::record(TblNum tblnum, const gcstring& tblname, int nrows, int nextfield, int totalsize)
	{
	Record r;
	r.addval(tblnum);
	r.addval(tblname);
	r.addval(nextfield);
	r.addval(nrows);
	r.addval(totalsize);
	*r.alloc(24) = 0; // 3 fields * max int packsize - min int packsize
	return r;
	}

// columns records
Record Database::record(TblNum tblnum, const gcstring& column, int field)
	{
	Record r;
	r.addval(tblnum);
	r.addval(column);
	r.addval(field);
	return r;
	}

// indexes records
Record Database::record(TblNum tblnum, const gcstring& columns, Index* index,
	const gcstring& fktable, const gcstring& fkcolumns, int fkmode)
	{
	Record r;
	r.addval(tblnum);
	r.addval(columns);
	if (index->iskey)
		r.addval(SuTrue);
	else
		r.addval(index->unique ? "u" : SuFalse);
	r.addval(fktable);
	r.addval(fkcolumns);
	r.addval(fkmode);
	index->getinfo(r);
	*r.alloc(24) = 0; // 3 updatable int fields * max int packsize - min int packsize
	return r;
	}

// tables keys
Record Database::key(TblNum tblnum)
	{
	Record r;
	r.addval(tblnum);
	return r;
	}
Record Database::key(const gcstring& table)
	{
	Record r;
	r.addval(table);
	return r;
	}
Record Database::key(const char* table)
	{
	Record r;
	r.addval(table);
	return r;
	}

// columns and indexes keys
Record Database::key(TblNum tblnum, const char* columns)
	{
	Record r;
	r.addval(tblnum);
	r.addval(columns);
	return r;
	}
Record Database::key(TblNum tblnum, const gcstring& columns)
	{
	Record r;
	r.addval(tblnum);
	r.addval(columns);
	return r;
	}

Index* Database::mkindex(const Record& r)
	{
	static Value u("u");

	verify(! nil(r));
	auto columns = r.getstr(I_COLUMNS).str();
	return new Index(this,
		r.getlong(I_TBLNUM),
		columns,
		r.getmmoffset(I_ROOT),
		r.getlong(I_TREELEVELS),
		r.getlong(I_NNODES),
		r.getval(I_KEY) == SuTrue,
		r.getval(I_KEY) == u);
	}

short* comma_to_nums(const Lisp<Col>& cols, const gcstring& str)
	{
	Array<short,100> nums;
	for (Lisp<gcstring> list = commas_to_list(str); ! nil(list); ++list)
		{
		Lisp<Col> p = cols;
		for (; ! nil(p); ++p)
			{
			Col& col = *p;
			if (col.column == *list)
				{
				if (col.colnum == -1)
					except("cannot index rule field " << col.column);
				nums.push_back(col.colnum);
				break ;
				}
			}
		if (nil(p))
			return 0;
		}
	nums.push_back(END);
	return nums.dup();
	}

Tbl* Database::ck_get_table(const gcstring& table)
	{
	Tbl* tbl = get_table(table);
	if (! tbl)
		except("nonexistent table: " << table);
	return tbl;
	}

Tbl* Database::get_table(const gcstring& table)
	{
	// if the table has already been read, return it
	if (Tbl* tbl = tables->get(table))
		{
		verify(tbl->name == table);
		return tbl;
		}
	return get_table(find(schema_tran, tables_index, key(table)));
	}

Tbl* Database::get_table(TblNum tblnum)
	{
	// if the table has already been read, return it
	if (Tbl* tbl = tables->get(tblnum))
		{
		verify(tbl->num == tblnum);
		return tbl;
		}
	return get_table(find(schema_tran, tblnum_index, key(tblnum)));
	}

Tbl* Database::get_table(const Record& table_rec)
	{
	if (nil(table_rec))
		return 0; // table not found
	gcstring table = table_rec.getstr(T_TABLE).to_heap();

	Record tblkey = key(table_rec.getlong(T_TBLNUM));

	// columns
	Lisp<Col> cols;
	verify(columns_index);
	for (auto iter = columns_index->begin(schema_tran, tblkey); ! iter.eof(); ++iter)
		{
		Record r(iter.data());
		gcstring column = r.getstr(C_COLUMN).to_heap();
		int colnum = r.getlong(C_FLDNUM);
		cols.push(Col(column, colnum));
		}
	cols.sort();

	// have to do this before indexes since they may need it
	Tbl* tbl = new Tbl(table_rec, cols, Lisp<Idx>());
	tables->add(tbl);

	// indexes
	Lisp<Idx> idxs;
	verify(indexes_index);
	for (auto iter = indexes_index->begin(schema_tran, tblkey); ! iter.eof(); ++iter)
		{
		Record r(iter.data());
		gcstring columns = r.getstr(I_COLUMNS).to_heap();
		short* colnums = comma_to_nums(cols, columns);
		verify(colnums);
		// make sure to use the same index for the system tables
		Index* index;
		if (table == "tables" && columns == "tablename")
			index = tables_index;
		else if (table == "tables" && columns == "table")
			index = tblnum_index;
		else if (table == "columns" && columns == "table,column")
			index = columns_index;
		else if (table == "indexes" && columns == "table,columns")
			index = indexes_index;
		else if (table == "indexes" && columns == "fktable,fkcolumns")
			index = fkey_index;
		else
			index = mkindex(r);
		idxs.push(Idx(table, r, columns, colnums, index, this));
		}
	tbl->idxs = idxs.reverse();
	return tbl;
	}

gcstring Database::get_view(const gcstring& table)
	{
	if (! views_index)
		views_index = get_index("views", "view_name");
	Record r = find(schema_tran, views_index, key(table));
	if (nil(r))
		return "";
	if (r.getstr(V_DEFINITION) == "")
		return " ";
	return r.getstr(V_DEFINITION).to_heap();
	}

void Database::remove_view(const gcstring& table)
	{
	remove_any_record(schema_tran, "views", "view_name", key(table));
	}

Index* Database::get_index(Tbl* tbl, const gcstring& columns)
	{
	if (! tbl)
		return 0;
	for (Lisp<Idx> p = tbl->idxs; ! nil(p); ++p)
		if (p->columns == columns)
			return p->index;
	return 0;
	}

Index* Database::first_index(const gcstring& table)
	{
	Tbl* tbl = get_table(table);
	if (! tbl || nil(tbl->idxs))
		return 0;
	return tbl->idxs->index;
	}

void Database::schema_out(Ostream& os, const gcstring& table)
	{
	Tbl* tbl = ck_get_table(table);

	// fields
	os << "(";
	Lisp<gcstring> f;
	int n = 0, i = 0;
	for (f = get_fields(table); ! nil(f); ++f, ++i)
		if (*f != "-")
			os << (n++ ? "," : "") << *f;
	for (f = get_rules(table); ! nil(f); ++f)
		os << (n++ ? "," : "") << f->capitalize();
	os << ")";

	// indexes
	for (Lisp<Idx> ix = tbl->idxs; ! nil(ix); ++ix)
		{
		if (ix->iskey)
			os << " key";
		else
			os << " index" << (ix->index->is_unique() ? " unique" : "");
		os << "(" << ix->columns << ")";
		if (ix->fksrc.table != "")
			{
			os << " in " << ix->fksrc.table;
			if (ix->fksrc.columns != ix->columns)
				os << "(" << ix->fksrc.columns << ")";
			if (ix->fksrc.mode == CASCADE)
				os << " cascade";
			else if (ix->fksrc.mode == CASCADE_UPDATES)
				os << " cascade update";
			}
		}
	}

Record project(const Record& r, short* cols, Mmoffset adr)
	{
	Record x;
	for (int i = 0; cols[i] != END; ++i)
		if (cols[i] < r.size())
			x.addraw(r.getraw(cols[i]));
		else
			x.addnil();
	if (adr)
		// need to add record address because even unique indexes
		// will have duplicates because of keeping old versions
		// for outstanding transactions to see
		x.addmmoffset(adr);
	return x;
	}

bool Database::rename_table(const gcstring& oldname, const gcstring& newname)
	{
	if (oldname == newname)
		return true;

	Tbl* tbl = ck_get_table(oldname);
	if (is_system_table(oldname))
		except("rename table: can't rename system table: " << oldname);
	if (istable(newname))
		except("rename table: table already exists: " << newname);

	update_any_record(schema_tran, "tables", "table", key(tbl->num),
		record(tbl->num, newname, tbl->nrecords, tbl->nextfield, tbl->totalsize));

	tables->erase(oldname); // Note: table will be reloaded into tables on next use

	return true;
	}

bool Database::rename_column(const gcstring& table, const gcstring& oldname, const gcstring& newname)
	{
	if (oldname == newname)
		return true;

	Tbl* tbl = ck_get_table(table);
	if (is_system_column(table, oldname))
		except("rename column: can't rename system column: " << oldname << " in " << table);


	Col* col = NULL;
	for (Lisp<Col> cols = tbl->cols; ! nil(cols); ++cols)
		if (cols->column == newname)
			except("rename column: column already exists: " << newname << " in " << table);
		else if (cols->column == oldname)
			col = &(*cols);
	if (! col)
		except("rename column: nonexistent column: " << oldname << " in " << table);

	// update columns table
	Record rec = record(tbl->num, newname, col->colnum);
	add_any_record(schema_tran, "columns", rec);
	col->column = newname;
	remove_any_record(schema_tran, "columns", "table,column", key(tbl->num, oldname));

	// update any indexes that include this column
	for (Lisp<Idx> idx = tbl->idxs; ! nil(idx); ++idx)
		{
		Lisp<gcstring> cols = commas_to_list(idx->columns);
		Lisp<gcstring> i = cols;
		for (; ! nil(i); ++i)
			if (*i == oldname)
				break ;
		if (nil(i))
			continue ; // this index doesn't contain the column

		*i = newname;
		gcstring newcolumns = list_to_commas(cols);
		Record rec2 = record(tbl->num, newcolumns, idx->index);
		add_any_record(schema_tran, "indexes", rec2);
		remove_any_record(schema_tran, "indexes", "table,columns", key(tbl->num, idx->columns));
		}

	tables->erase(table); // Note: table will be reloaded into tables on next use
	return true;
	}

int Database::nrecords(const gcstring& table)
	{
	Tbl* tbl = ck_get_table(table);
if (tbl->nrecords < 0) return 0;
	return tbl->nrecords;
	}

size_t Database::totalsize(const gcstring& table)
	{
	Tbl* tbl = ck_get_table(table);
// workaround for negative totalsize bug
if (tbl->totalsize < 0) return 0;
	return tbl->totalsize;
	}

float Database::rangefrac(const gcstring& table, const gcstring& index, const Record& org, const Record& end)
	{
	Index* idx = get_index(table, index);
	verify(idx);
	return idx->rangefrac(org, end);
	}

Record Database::get_record(int tran, const gcstring& table, const gcstring& index, const Record& key)
	{
	return find(tran, get_index(table, index), key);
	}

// testing ==========================================================

#include "testing.h"
#include "tempdb.h"
#include "btree.h"

static void assertreceq(const Record& r1, const Record& r2)
	{
	if (r1.size() != r2.size())
		except(r1 << endl << "!=" << endl << r2);
	for (int i = 1; i < r1.size(); ++i)
		if (r1.getraw(i) != r2.getraw(i))
			except(r1 << endl << "!=" << endl << r2);
	}

static Record record(const char* s1, const char* s2, int n)
	{
	Record r;
	r.addval(s1);
	r.addval(s2);
	r.addval(n);
	return r;
	}
static Record key(int n)
	{
	Record r;
	r.addval(n);
	return r;
	}

#define BEGIN \
	{ TempDB tempdb;
#define END \
	verify(thedb->final_empty()); \
	verify(thedb->trans_empty()); \
	}

TEST(database)
	{
	BEGIN

	thedb->add_table("test_database");
	thedb->add_column("test_database", "name");
	thedb->add_column("test_database", "phone");
	thedb->add_column("test_database", "num");
	verify(thedb->get_columns("test_database") == lisp(gcstring("name"), gcstring("phone"), gcstring("num")));

	thedb->add_index("test_database", "name", true);
	thedb->add_index("test_database", "num", true);

	Record records[] =
		{
		record("bob", "123-4444", 1),
		record("joe", "652-9876", 2),
		record("axon", "249-5050", 3),
		record("andrew", "242-0707", 4),
		record("bobby", "123-4567", 5)
		};
	const int nrecs = sizeof (records) / sizeof (Record);

	int tran = thedb->transaction(READWRITE);

	// add records
	int i;
	for (i = 0; i < nrecs; ++i)
		thedb->add_record(tran, "test_database", records[i]);

	// adding duplicates should fail
	for (i = 0; i < nrecs; ++i)
		xassert(thedb->add_record(tran, "test_database", records[i]));
	xassert(thedb->add_record(tran, "test_database", record("new", "", 3)));

	verify(thedb->commit(tran));

	tran = thedb->transaction(READONLY);

	Index* index = thedb->get_index("test_database", "name");
	Index::iterator iter = index->begin(tran);
	const int nameorder[] = { 3, 2, 0, 4, 1 };
	for (i = 0; ! iter.eof(); ++iter, ++i)
		{
		verify(i < nrecs);
		assertreceq(records[nameorder[i]], Record(iter.data()));
		}
	assert_eq(i, nrecs);

	thedb->add_index("test_database", "phone", true);

	const int phoneorder[] = { 0, 4, 3, 2, 1};
	index = thedb->get_index("test_database", "phone");
	iter = index->begin(tran);
	for (i = 0; ! iter.eof(); ++iter, ++i)
		{
		verify(i < nrecs);
		assertreceq(records[phoneorder[i]], Record(iter.data()));
		}

	verify(thedb->commit(tran));

	// update
	tran = thedb->transaction(READWRITE);
	xassert(thedb->update_record(tran, "test_database", "num",
		key(2), record("joe", "652-9876", 4)));
	thedb->update_record(tran, "test_database", "num",
		key(3), record("axon", "249-5051", 3));
	thedb->update_record(tran, "test_database", "num",
		key(3), record("axon", "249-5050", 3));
	verify(thedb->commit(tran));
	// verify
	tran = thedb->transaction(READONLY);
	iter = index->begin(tran);
	for (i = 0; ! iter.eof(); ++iter, ++i)
		{
		verify(i < nrecs);
		assertreceq(records[phoneorder[i]], Record(iter.data()));
		}
	assert_eq(i, nrecs);
	verify(thedb->commit(tran));


	xassert(thedb->remove_column("test_database", "name"));

	thedb->add_column("test_database", "extra");
	thedb->add_index("test_database", "extra", false);
	xassert(thedb->remove_column("test_database", "extra"));
	thedb->remove_index("test_database", "extra");
	thedb->remove_column("test_database", "extra");
	thedb->remove_table("test_database");

	END
	}

TEST(database_rules)
	{
	BEGIN

	int tran = thedb->transaction(READWRITE);

	const char* table = "test_database";
	thedb->add_table(table);
	thedb->add_column(table, "one");
	thedb->add_column(table, "Two"); // rule
	thedb->add_column(table, "three");
	thedb->add_index(table, "one", true);
	assert_eq(thedb->get_fields(table), lisp(gcstring("one"), gcstring("three")));
	assert_eq(thedb->get_columns(table), lisp(gcstring("two"), gcstring("one"), gcstring("three")));
	assert_eq(thedb->get_rules(table), lisp(gcstring("two")));
	Record r;
	r.addval("one");
	r.addval(3);
	thedb->add_record(tran, table, r);
	Index* index = thedb->get_index(table, "one");
	verify(index);
	Index::iterator iter = index->begin(tran);
	verify(! iter.eof());
	Record r2(iter.data());
	assertreceq(r, r2);
	++iter;
	verify(iter.eof());

	verify(thedb->commit(tran));

	END
	}

TEST(database_create)
	{
	BEGIN

	tempdb.reopen();

	int n;
	Index* index;
	Index::iterator iter;

	n = 0;
	index = thedb->get_index("indexes", "table,columns");
	for (iter = index->begin(schema_tran); ! iter.eof(); ++iter)
		{ ++n; }
	assert_eq(n, 6);

	END
	}

TEST(database_delete_rule_bug)
	{
	BEGIN
	const char* table = "test_database";
	thedb->add_table(table);
	thedb->add_column(table, "a");
	thedb->add_column(table, "B"); // rule
	thedb->add_column(table, "C");
	thedb->add_column(table, "D");
	thedb->add_index(table, "a", true);

	thedb->remove_column(table, "c");
	thedb->add_column(table, "c");
	END
	}

