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

// NOTE: offsets in commits are to data, which is _preceded_ by 4 byte table#
// but Mmfile iterator offsets are to start of _block_

#include "recover.h"
#include "database.h"
#include "suboolean.h"
#include "ostreamfile.h"
#include "alert.h"
#include <vector>
#include "catstr.h"
#include <stdio.h>	// for remove
#include "minmax.h"
#include "errlog.h"
#include "checksum.h"
#include "value.h"
#include "cmdlineoptions.h"
#include "ostreamstr.h"
#include "exceptimp.h"

#ifdef _MSC_VER
typedef std::_Bvector BitVector;
#else
typedef std::vector<bool> BitVector;
#endif

static bool alerts = false;

bool check_shutdown(Mmfile* mmf)
	{
	if (cmdlineoptions.ignore_check)
		return true;
	void* p = *--mmf->end();
	return (mmf->type(p) == MM_SESSION && ((Session*) p)->mode == Session::SHUTDOWN);
	}

const int granularity = 16; // 8 byte mm overhead + 8 byte min size

class TrEntry
	{
public:
	TrEntry(Mmoffset f, Mmoffset t) : from_(f), to_(t)
		{ }
	Mmoffset from() const
		{ return from_.unpack(); }
	Mmoffset to() const
		{ return to_.unpack(); }
private:
	Mmoffset32 from_;
	Mmoffset32 to_;
	};
inline bool operator<(const TrEntry& te, Mmoffset off)
	{ return te.from() < off; }
inline bool operator<(Mmoffset off, const TrEntry& te)
	{ return off < te.from(); }

class Translate
	{
public:
	Translate(int n)
		{ map.reserve(n); }
	void add(Mmoffset from, Mmoffset to)
		{ map.push_back(TrEntry(from, to)); }
	void pop()
		{ map.pop_back(); }
	Mmoffset operator[](Mmoffset from)
		{
		std::vector<TrEntry>::iterator it = std::lower_bound(map.begin(), map.end(), from);
		verify(it->from() == from);
		return it->to();
		}
private:
	std::vector<TrEntry> map;
	};

int& tblnum(Mmfile* mmf, Mmoffset off)
	{ return ((int*) mmf->adr(off))[-1]; }

static bool schema(Database& db, HashMap<TblNum,gcstring>& tblnames, Mmoffset o);
static void dbdump1(Ostream& log, Mmfile& mmf, Mmfile::iterator& iter, ulong& cksum, BitVector& deletes);

static int max_tblnum = 0;

Mmfile* open_mmf(char* file)
	{
	Mmfile* mmf = new Mmfile(file);
	// old file is set to smaller max chunks mapped
	// because it is read in a single pass
	mmf->set_max_chunks_mapped(64);
	return mmf;
	}

class DbRecoverImp : public DbRecover
	{
public:
	DbRecoverImp(char* f) : file(f), mmf(open_mmf(file)),
		deletes(mmf->size() / granularity),
		ndata(0), last_good(0), status_(DBR_UNRECOVERABLE)
		{ }
	~DbRecoverImp()
		{ delete mmf; mmf = 0; }
	void docheck(bool (*progress)(int));
	DbrStatus check_indexes(bool (*progress)(int));
	DbrStatus status()
		{ return status_; }
	virtual char* last_good_commit();
	virtual bool rebuild(bool (*progress)(int), bool log = true);
private:
	bool rebuild_copy(char* newfile, bool (*progress)(int));

	char* file;
	Mmfile* mmf;
	BitVector deletes;
	int ndata;
	Mmfile::iterator end;
	time_t last_good;
	DbrStatus status_;
	};

DbRecover* DbRecover::check(char* file, bool (*progress)(int))
	{
	DbRecoverImp* dbr = new DbRecoverImp(file);
	dbr->docheck(progress);
	return dbr;
	}

bool null_progress(int)
	{ return true; }

void DbRecoverImp::docheck(bool (*progress)(int))
	{
	progress(0);
	int step = max<int>(1, mmf->size() / 50);
	int lastpos = 0;

	if (! mmf->first())
		{
		status_ = DBR_UNRECOVERABLE;
		return ;
		}

	//	- find last good commit
	//	- track all the deletes
	time_t last_good_shutdown = 0;
	ulong cksum = checksum(0, 0, 0);
	bool ok = false;
	HashMap<int,int> tbl_nextfield;
	Mmfile::iterator iter = mmf->begin();
	for (; iter != mmf->end(); ++iter)
		{
		if (iter.offset() - lastpos > step)
			{
			if (! progress(iter.offset() / step))
				return ;
			lastpos = iter.offset();
			}
		ok = false;
		if (iter.type() == MM_DATA)
			{
			++ndata;
			int tblnum = *(int*) *iter;
			if (tblnum == TN_TABLES)
				{
				Record r(mmf, iter.offset() + sizeof (int));
				tbl_nextfield[r.getlong(T_TBLNUM)] = r.getlong(T_NEXTFIELD);
				}
			if (tblnum != TN_TABLES && tblnum != TN_INDEXES)
				{
				Record r(mmf, iter.offset() + sizeof (int));
				static bool fieldsbug = false;
				if (! fieldsbug && ! cmdlineoptions.unattended &&
					tbl_nextfield.find(tblnum) && r.size() > tbl_nextfield[tblnum])
					{
					alerts = true;
					alert("data record has too many fields\n\nrun: suneido -compact");
					fieldsbug = true;
					}
				cksum = checksum(cksum, *iter, sizeof (int) + r.cursize());
				}
			}
		else if (iter.type() == MM_COMMIT)
			{
			Commit* commit = (Commit*) *iter;
			cksum = checksum(cksum, (char*) commit + sizeof (long),
				iter.size() - sizeof (long));
			if (commit->cksum != cksum)
				break ;
			end = iter;
			cksum = checksum(0, 0, 0);
			Mmoffset32* dels = commit->deletes();
			for (int i = 0; i < commit->ndeletes; ++i)
				{
				Mmoffset off = dels[i].unpack();
				verify(off % 8 == 0);
				deletes[(off - sizeof (long)) / granularity] = true;
				// - sizeof (long) because these offsets are to data, which is preceded by table number
				}
			}
		else if (iter.type() == MM_SESSION)
			{
			Session* session = (Session*) *iter;
			if (session->mode == Session::SHUTDOWN)
				{
				last_good_shutdown = session->t;
				ok = true;
				}
			cksum = checksum(0, 0, 0);
			}
		}
	progress(50);

	if (iter.corrupt())
		ok = false;
	if (last_good_shutdown == 0)
		{
		status_ = DBR_UNRECOVERABLE; // no valid shutdowns
		return ;
		}
	if (*end == 0)
		{
		status_ = DBR_UNRECOVERABLE; // no valid commits
		return ;
		}
	last_good = ((Commit*) *end)->t;
	if (! ok)
		status_ = DBR_ERROR;
	else
		status_ = DBR_OK;

	++end;
	}

DbrStatus DbRecoverImp::check_indexes(bool (*progress)(int))
	{
	delete mmf; mmf = 0;
	Database db(file);
	Tbl* tbl = db.ck_get_table("tables");
	int n_tables = tbl->nrecords;
	Index* index = db.get_index("tables", "table");
	verify(index);
	Index::iterator tbl_iter = index->begin(schema_tran);
	for (int i = 0; ! tbl_iter.eof(); ++tbl_iter, ++i)
		{
		if (! progress(50 + i * 100 / n_tables))
			return DBR_OK;
		Record tablerec(tbl_iter.data());
		gcstring table = tablerec.getstr(T_TABLE);
		Tbl* tbl = db.ck_get_table(table);
		int64 prev_cksum = 0;
		for (Lisp<Idx> ix = tbl->idxs; ! nil(ix); ++ix)
			{
			int n = 0;
			int64 cksum = 0;
			Index::iterator iter = ix->index->begin(schema_tran);
			for (; ! iter.eof(); ++iter, ++n)
				{
				cksum += iter->adr();
				Record rec(iter.data());
				Record key = project(rec, ix->colnums, rec.off());
				if (iter->key != key)
					{
					OstreamStr os;
					os << "check indexes: " << tbl->name << " " << ix->columns << " data does not match index";
					errlog(os.str());
					status_ = DBR_ERROR;
					break ;
					}
				}
			if (n != tbl->nrecords)
				{
				OstreamStr os;
				os << "check indexes: " << tbl->name << " " << tbl->nrecords << " != " << ix->columns << " " << n;
				errlog(os.str());
				status_ = DBR_ERROR;
				break ;
				}
			if (prev_cksum && cksum != prev_cksum)
				{
				OstreamStr os;
				os << "check indexes: " << tbl->name << " checksums do not match";
				errlog(os.str());
				status_ = DBR_ERROR;
				break ;
				}
			prev_cksum = cksum;
			}
		}
	progress(100);
	return status_;
	}

char* DbRecoverImp::last_good_commit()
	{
	char* s = ctime(&last_good);
	if (! s)
		return "";
	s[strlen(s) - 1] = 0; // strip \n
	return strdup(s); // dup since ctime is static
	}

bool DbRecoverImp::rebuild(bool (*progress)(int), bool log)
	{
	if (! mmf)
		mmf = open_mmf(file);
	char newfile[L_tmpnam] = "";
	if (! rebuild_copy(newfile, progress))
		{
		remove(newfile);
		errlog("Rebuild failed", last_good_commit());
		return false;
		}
	delete mmf; mmf = 0; // close it
	char* bakfile = CATSTRA(file, ".bak");
	remove(bakfile);
	verify(0 == rename(file, bakfile));
	verify(0 == rename(newfile, file));

	if (log)
		errlog("Rebuild as of", last_good_commit());
	return true;
	}

bool DbRecoverImp::rebuild_copy(char* newfile, bool (*progress)(int))
	{
	progress(0);
	int step = mmf->size() / 100;
	int lastpos = 0;

	Database db(newfile, DBCREATE);

	ulong cksum = checksum(0, 0, 0);
	Translate tr(ndata);
	HashMap<TblNum,gcstring> tblnames;
	HashMap<TblNum,Mmoffset> deleted_tbls, renamed_tbls;
	// hide any records added (not copied) by rebuild
	db.output_type = MM_OTHER;
	for (Mmfile::iterator iter = mmf->begin(); iter != end; ++iter)
		{
		if (iter.offset() - lastpos > step)
			{
			if (! progress(iter.offset() / step))
				return false;
			lastpos = iter.offset();
			}
		if (iter.type() != MM_OTHER)
			{
			Mmoffset o = db.alloc(iter.size(), iter.type());
			void* p = db.adr(o);
			memcpy(p, *iter, iter.size());
			if (iter.type() == MM_DATA)
				{
				Mmoffset oldoff = iter.offset();
				tr.add(oldoff, o);
				TblNum tblnum = *(int*) p;
				verify(oldoff % 8 == 4);
				bool deleted = deletes[oldoff / granularity];
				o += sizeof (int); // tblnum

				// handle renamed tables
				if (tblnum == TN_TABLES && deleted)
					{
					Record r(db.mmf, o);
					int tn = r.getlong(T_TBLNUM);
					deleted_tbls[tn] = o;
					}
				else if (tblnum == TN_INDEXES && ! deleted)
					{
					Record r(db.mmf, o);
					int tn = r.getlong(I_TBLNUM);
					if (Mmoffset* po = deleted_tbls.find(tn))
						{
						schema(db, tblnames, *po);
						renamed_tbls[tn] = *po;
						deleted_tbls.erase(tn);
						}
					}
				else if (tblnum == TN_TABLES && ! deleted)
					{
					Record r(db.mmf, o);
					int tn = r.getlong(I_TBLNUM);
					if (Mmoffset* po = renamed_tbls.find(tn))
						{
						gcstring oldname = Record(db.mmf, *po).getstr(T_TABLE);
						gcstring newname = r.getstr(T_TABLE).to_heap();
						db.unalloc(iter.size());
						tr.pop();
						db.rename_table(oldname, newname);
						renamed_tbls.erase(tn);
						deleted = true; // to prevent schema(...) below
						}
					else
						{
						deleted_tbls.erase(tn);
						}
					}
				// end of handling renamed tables

				if (tblnum <= TN_VIEWS && ! deleted &&
					! schema(db, tblnames, o))
					{
					// if schema fails, remove from new database
					// this is to prevent "garbage" being copied to new database
					// e.g. IDX or COL for nonexistent table
					db.unalloc(iter.size());
					tr.pop();
					}
				else if (tblnum != TN_TABLES && tblnum != TN_INDEXES)
					{
					Record r(db.mmf, o);
					cksum = checksum(cksum, *iter, sizeof (int) + r.cursize());
					}
				}
			else if (iter.type() == MM_SESSION)
				{
				cksum = checksum(0, 0, 0);
				}
			else if (iter.type() == MM_COMMIT)
				{
				Commit* commit = (Commit*) p;
				Mmoffset32* creates = commit->creates();
				int i = 0;
				for (i = 0; i < commit->ncreates; ++i)
					{
					Mmoffset oldoff = creates[i].unpack();
					Mmoffset newoff = tr[oldoff - sizeof (long)] + sizeof (long);
					creates[i] = newoff;
					TblNum tn = tblnum(mmf, oldoff);
					gcstring table = tblnames[tn];
					if (table == "" || tn <= TN_VIEWS)
						continue ;
					Record r(db.mmf, newoff);
					if (! deletes[(oldoff - sizeof (long)) / granularity])
						{
						try
							{
							Tbl* tbl = db.ck_get_table(table);
							db.add_index_entries(schema_tran, tbl, r);
							}
						catch (const Except& e)
							{
							alerts = true;
							alert("ignoring: " << table << ": " << e);
							}
						}
					}
				Mmoffset32* deletes = commit->deletes();
				for (i = 0; i < commit->ndeletes; ++i)
					deletes[i] = tr[deletes[i].unpack() - sizeof (long)] + sizeof (long);
				commit->cksum = checksum(cksum, (char*) commit + sizeof (long),
					iter.size() - sizeof (long));
				cksum = checksum(0, 0, 0);
				}
			}
		}
	db.dbhdr()->next_table = max_tblnum + 1;

	progress(100);
	return true;
	}

// process schema records
static bool schema(Database& db, HashMap<TblNum,gcstring>& tblnames, Mmoffset o)
	{
	Record r(db.mmf, o);
	TblNum schema_tblnum = ((int*) db.mmf->adr(o))[-1];
	TblNum tblnum;
	gcstring table;
	switch (schema_tblnum)
		{
	case TN_TABLES :
		tblnum = r.getlong(T_TBLNUM);
		if (tblnum <= TN_VIEWS)
			break ;
		if (tblnum > max_tblnum)
			max_tblnum = tblnum;
		table = r.getstr(T_TABLE).to_heap();
		tblnames[tblnum] = table;
		try
			{
			db.add_index_entries(schema_tran, db.get_table(TN_TABLES), r);
			}
		catch (const Except& e)
			{
			alerts = true;
			alert("ignoring: " << table << ": " << e);
			}
		// reset sizes
		r.reuse(T_NROWS);
		r.addval((long) 0);		// nrows
		r.addval((long) 100);	// totalsize
		break ;
	case TN_COLUMNS :
		tblnum = r.getlong(C_TBLNUM);
		if (tblnum <= TN_VIEWS)
			break ;
		db.add_index_entries(schema_tran, db.get_table(TN_COLUMNS), r);
		db.invalidate_table(r.getlong(C_TBLNUM));
		break ;
	case TN_INDEXES :
		tblnum = r.getlong(I_TBLNUM);
		if (tblnum <= TN_VIEWS)
			break ;
		if (! db.recover_index(r))
			{
			table = tblnames[tblnum];
			gcstring columns = r.getstr(I_COLUMNS);
			bool key = r.getval(I_KEY) == SuTrue;
			alert("recover index failed: " << tblnum << "=" << table << (key ? " key(" : " index(") << columns << ")");
			return false;
			}
		break ;
	case TN_VIEWS :
		db.add_index_entries(schema_tran, db.get_table(TN_VIEWS), r);
		break ;
	default :
		unreachable();
		}
	return true;
	}

static void schema_dump(void* p, Ostream& log)
	{
	Record r((char*) p + sizeof (int));
	TblNum tblnum;
	gcstring table;
	int nrows;
	log << "\t";
	switch (*(int*) p)
		{
	case TN_TABLES :
		{
		tblnum = r.getlong(T_TBLNUM);
		table = r.getstr(T_TABLE);
		nrows = r.getlong(T_NROWS);
		log << "TBL " << table << " = " << tblnum << " nrows " << nrows << endl;
		break ;
		}
	case TN_COLUMNS :
		{
		tblnum = r.getlong(C_TBLNUM);
		gcstring column = r.getstr(C_COLUMN);
		log << "COL " << tblnum << ", " << column << endl;
		break ;
		}
	case TN_INDEXES :
		{
		tblnum = r.getlong(I_TBLNUM);
		if (tblnum <= TN_VIEWS)
			break ;
		gcstring columns = r.getstr(I_COLUMNS);
		bool key = r.getval(I_KEY) == SuTrue;
		gcstring fktable = r.getstr(I_FKTABLE);
		gcstring fkcolumns = r.getstr(I_FKCOLUMNS);
		log << "IDX " << tblnum << ", " << (key ? "key" : "index") << "(" << columns << ")";
		if (fktable != "")
			{
			log << " in " << fktable;
			if (fkcolumns != columns)
				log << "(" << fkcolumns << ")";
			}
		log << endl;
		break ;
		}
	case TN_VIEWS :
		{
		table = r.getstr(V_NAME);
		gcstring definition = r.getstr(V_DEFINITION);
		log << "VIEW " << table << endl;
		break ;
		}
	default :
		unreachable();
		}
	}

void dbdump(char* db, bool append)
	{
	Mmfile mmf(db, 0);
	BitVector deletes(mmf.size() / granularity);
	Mmfile::iterator iter;
	const Mmfile::iterator end = mmf.end();
	for (iter = mmf.begin(); iter != end; ++iter)
		{
		if (iter.type() == MM_COMMIT)
			{
			Commit* commit = (Commit*) *iter;
			Mmoffset32* dels = commit->deletes();
			for (int i = 0; i < commit->ndeletes; ++i)
				deletes[(dels[i].unpack() - 4) / granularity] = true;
			}
		}
	OstreamFile log("dbdump.log", append ? "a" : "w");
	ulong cksum = checksum(0, 0, 0);
	for (iter = mmf.begin(); iter != end; ++iter)
		{
		dbdump1(log, mmf, iter, cksum, deletes);
		}
	if (iter.corrupt())
		log << "CORRUPT" << endl;
	}

void dbdump1(Ostream& log, Mmfile& mmf, Mmfile::iterator& iter, ulong& cksum, BitVector& deletes)
	{
	log << iter.offset() << " " << iter.size() << "\t";
	switch (iter.type())
		{
	case MM_DATA :
		{
		int tbl = *(int*) *iter;
		log << "data " << tbl;
		if (deletes[iter.offset() / granularity])
			log << " deleted";
		log << endl;
		if (tbl != TN_TABLES && tbl != TN_INDEXES)
			{
			Record r((char*) *iter + sizeof (int));
			cksum = checksum(cksum, *iter, sizeof (int) + r.cursize());
			}
		TblNum tblnum = *(int*) *iter;
		if (tblnum <= TN_VIEWS)// && ! deletes[iter.offset() / granularity])
			schema_dump(*iter, log);
		break ;
		}
	case MM_COMMIT :
		{
		Commit* commit = (Commit*) *iter;
		log << "commit "
			<< "creates " << commit->ncreates << ", "
			<< "deletes " << commit->ndeletes << ", "
			<< ctime(&commit->t);
		/*{
		Mmoffset32* p;
		Mmoffset32* end;
		for (p = commit->creates(), end = p + commit->ncreates; p < end; ++p)
			log << "\tcreate " << p->unpack() - 4 << endl;
		for (p = commit->deletes(), end = p + commit->ndeletes; p < end; ++p)
			log << "\tdelete " << p->unpack() - 4 << endl;
		}*/
		cksum = checksum(cksum, (char*) commit + sizeof (long),
			iter.size() - sizeof (long));
		if (commit->cksum != cksum)
			log << "	ERROR commit had " << commit->cksum <<
				" recover got " << cksum << endl;
		cksum = checksum(0, 0, 0);
		break ;
		}
	case MM_SESSION :
		{
		Session* session = (Session*) *iter;
		log << "session " <<
			(session->mode == Session::STARTUP ? "startup" : "shutdown") <<
			" " << ctime(&session->t);
		cksum = checksum(0, 0, 0);
		break ;
		}
	case MM_OTHER :
	default :
		log << "other" << endl;
		}
	}

#include "testing.h"
#include "tempdb.h"

struct Cleanup
	{
	Cleanup()
		{ }
	~Cleanup()
		{
		remove("tempdb");
		remove("tempdb.bak");
		}
	};

class test_recover : public Tests
	{
	TEST(1, recreate)
		{
		Cleanup cleanup;
		{ TempDB tempdb(false);
		const char* table = "test_table";
		create(table);
		destroy(table);
		create(table);
		output(table, 10);
		}

		rebuild();
		}
	TEST(2, rename)
		{
		Cleanup cleanup;
		TempDB tempdb(false);
		const char* table = "test_table";
		const char* table2 = "xtest_table";
		create(table);
		output(table, 10);
		thedb->rename_table(table, table2);

		tempdb.close();
		rebuild();
		tempdb.open();

		verify(thedb->get_table(table2));
		verify(! thedb->get_table(table));
		}
	TEST(3, rename_create_destroy)
		{
		TempDB tempdb(false);
		Cleanup cleanup;
		const char* table = "test_table";
		const char* table2 = "xtest_table";
		create(table);
		output(table, 10);
		dostuff(table, table2);
		tempdb.close();
		rebuild();
		check();
		tempdb.open();
		dostuff(table, table2);
		tempdb.close();
		rebuild();
		check();
		}
	void create(const char* table)
		{
		thedb->add_table(table);
		thedb->add_column(table, "one");
		thedb->add_column(table, "Two"); // rule
		thedb->add_column(table, "three");
		thedb->add_index(table, "one", true);
		}
	void output(const char* table, int n)
		{
		int tran = thedb->transaction(READWRITE);
		for (int i = 0; i < n; ++i)
			thedb->add_record(tran, table, record(i));
		verify(thedb->commit(tran));
		}
	Record record(int n)
		{
		Record r;
		r.addval(n);
		return r;
		}
	void dostuff(const char* table, const char* table2)
		{
		thedb->rename_table(table, table2);
		create(table);
		output(table, 1);
		destroy(table);
		thedb->rename_table(table2, table);
		}
	void check()
		{
		DbRecover* dbr = DbRecover::check("tempdb");
		asserteq(dbr->status(), DBR_OK);
		dbr->check_indexes();
		asserteq(dbr->status(), DBR_OK);
		verify(! alerts);
		delete dbr;
		}
	void rebuild()
		{
		DbRecover* dbr = DbRecover::check("tempdb");
		asserteq(dbr->status(), DBR_OK);
		dbr->check_indexes();
		asserteq(dbr->status(), DBR_OK);
		verify(dbr->rebuild(null_progress, false));
		verify(! alerts);
		delete dbr;
		}
	void destroy(const char* table)
		{
		thedb->remove_table(table);
		}

	TEST(9, translate)
		{
		Translate tr(10);
		tr.add(80, 88);
		asserteq(tr[80], 88);
		const int64 MB = 1024 * 1024;
		tr.add(4500 * MB, 4600 * MB);
		asserteq(tr[4500 * MB], 4600 * MB);
		asserteq(tr[80], 88);
		}
	};
REGISTER(test_recover);
