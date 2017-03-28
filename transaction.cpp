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
#include "hashmap.h"
#include <limits.h>
#include "lisp.h"
#include "except.h"
#include "exceptimp.h"
#include "recover.h"
#include "errlog.h"
#include "checksum.h"
#include "ostreamstr.h"

using namespace std;

// TODO: why is trans a map? wouldn't a HashMap be faster & smaller?
// don't seem to use the ordered aspect

const TranTime FUTURE = INT_MAX;
const TranTime UNCOMMITTED = INT_MAX / 2;
const TranTime PAST = INT_MIN;

inline TranDelete::TranDelete(TranTime t) : tran(t), time(t + UNCOMMITTED)
	{ }

Transaction::Transaction()
	{ }

Transaction::Transaction(TranType t, TranTime clock, const char* sid)
	: type(t), tran(clock), asof(clock), session_id(sid)
	{ }

int Database::transaction(TranType type, const char* session_id)
	{
	if ((clock % 2) != type)
		++clock;
	trans[clock] = Transaction(type, clock, session_id);
	return clock++;
	}

Transaction* Database::get_tran(int tran)
	{
	map<int,Transaction>::iterator iter = trans.find(tran);
	return iter == trans.end() ? 0 : &iter->second;
	}

Transaction* Database::ck_get_tran(int tran)
	{
	Transaction* t = get_tran(tran);
	verify(t);
	return t;
	}

Lisp<int> Database::tranlist()
	{
	Lisp<int> ts;
	for (map<int,Transaction>::iterator iter = trans.begin(); iter != trans.end(); ++iter)
		ts.push(iter->first);
	return ts.reverse();
	}

int Database::final_size()
	{
	return final.size();
	}

// track actions ====================================================

void Database::create_act(int tran, TblNum tblnum, Mmoffset off)
	{
	if (tran == schema_tran)
		return ;
	Transaction* t = ck_get_tran(tran);
	verify(t->type == READWRITE);
	created[off] = UNCOMMITTED + tran;
	t->acts.push_back(TranAct(CREATE_ACT, tblnum, off, clock));
	}

bool Database::delete_act(int tran, TblNum tblnum, Mmoffset off)
	{
	if (tran == schema_tran)
		{
		schema_deletes.push(off);
		return true;
		}
	Transaction* t = ck_get_tran(tran);
	verify(t->type == READWRITE);
	if (TranDelete* p = deleted.find(off))
		{
		t->conflict = write_conflict(tblnum, off, p);
		t->asof = FUTURE;
		verify(finalize());
		return false;
		}
	deleted[off] = TranDelete(tran);
	t->acts.push_back(TranAct(DELETE_ACT, tblnum, off, clock));
	return true;
	}

char* Database::write_conflict(TblNum tblnum, Mmoffset a, TranDelete* p)
	{
	OstreamStr os;
	os << "write conflict with";
	const Transaction* t = find_tran(p->tran);
	if (t && t->session_id && *t->session_id)
		os << ' ' << t->session_id;
	os << " transaction " << p->tran;
	if (Tbl* tbl = get_table(tblnum))
		{
		os << ' ' << tbl->name;
		Lisp<Idx> idxs = tbl->idxs;
		for (; ! nil(idxs) && ! idxs->iskey; ++idxs)
			{ }
		if (! nil(idxs))
			os << " " << idxs->columns << ": " << project(input(a), idxs->colnums);
		}
	else
		os << " table " << tblnum;
	return os.str();
	}

struct FindTran
	{
	FindTran(int t) : tran(t)
		{ }
	bool operator()(const Transaction& t)
		{ return t.tran == tran; }
	int tran;
	};

const Transaction* Database::find_tran(int tran)
	{
	if (Transaction* t = get_tran(tran))
		return t;
	set<Transaction>::iterator it = find_if(final.begin(), final.end(), FindTran(tran));
	if (it != final.end())
		return &*it;
	return NULL;
	}

void Database::undo_delete_act(int tran, TblNum tblnum, Mmoffset off)
	{
	verify(deleted[off].tran == tran);
	Transaction* t = ck_get_tran(tran);
	verify(t->type == READWRITE);
	TranAct& ta = t->acts.back();
	verify(ta.type == DELETE_ACT && ta.tblnum == tblnum && ta.off == off);
	deleted.erase(off);
	t->acts.pop_back();
	}

void Database::table_create_act(TblNum tblnum)
	{
	++clock;
	table_created[tblnum] = clock;
	++clock;
	}

TranRead* Database::read_act(int tran, TblNum tblnum, const char* index)
	{
	if (tran == schema_tran)
		return 0;
	Transaction* t = ck_get_tran(tran);
	if (t->type == READONLY)
		return 0;
	t->reads.push_back(TranRead(tblnum, index));
	return &t->reads.back();
	}

// commit / abort ===================================================

bool Database::commit(int tran, const char** conflict)
	{
	Transaction* t = get_tran(tran);
	if (! t)
		return false;
	if (t->conflict)
		{
		abort(tran);
		if (conflict)
			*conflict = t->conflict;
		return false;
		}
	if (t->type == READWRITE && ! t->acts.empty())
		{
		if (! validate_reads(t))
			{
			abort(tran);
			if (conflict)
				*conflict = t->conflict;
			return false;
			}
		commit_update_tran(tran);
		}
	verify(trans.erase(tran));
	verify(finalize());
	return true;
	}

void Database::commit_update_tran(int tran)
	{
	Transaction* t = ck_get_tran(tran);
	int ncreates = 0;
	int ndeletes = 0;
	TranTime commit_time = clock++;
	for (deque<TranAct>::iterator act = t->acts.begin(); act != t->acts.end(); ++act)
		{
		switch (act->type)
			{
		case CREATE_ACT :
			{
			TranTime* p = created.find(act->off);
			verify(p && *p > UNCOMMITTED);
			*p = commit_time;
			++ncreates;
			break ;
			}
		case DELETE_ACT :
			{
			TranDelete* p = deleted.find(act->off);
			verify(p && p->time > UNCOMMITTED);
			p->time = commit_time;
			++ndeletes;
			break ;
			}
		default :
			unreachable();
			}
		}
	t->asof = commit_time;
	t->reads.clear(); // no longer needed
	final.insert(*t);

	write_commit_record(tran, t->acts, ncreates, ndeletes);
	}

void Database::write_commit_record(int tran, const deque<TranAct>& acts, int ncreates, int ndeletes)
	{
	// write out commit record
	ndeletes += schema_deletes.size();
	if (ndeletes || ncreates || cksum != ::checksum(0,0,0))
		{
		size_t n = sizeof (Commit) + (ncreates + ndeletes) * sizeof (Mmoffset32);
		Commit* commit = new (adr(alloc(n, MM_COMMIT))) Commit(tran, ncreates, ndeletes);
		Mmoffset32* creates = commit->creates();
		Mmoffset32* deletes = commit->deletes();
		for (deque<TranAct>::const_iterator act = acts.begin(); act != acts.end(); ++act)
			{
			if (act->type == CREATE_ACT)
				*creates++ = act->off;
			else // act->type == DELETE_ACT
				*deletes++ = act->off;
			}
		for (Lisp<Mmoffset> sdels = schema_deletes; ! nil(sdels); ++sdels)
			*deletes++ = *sdels;
		verify(creates - commit->creates() == ncreates);
		verify(deletes - commit->deletes() == ndeletes);
		schema_deletes = Lisp<Mmoffset>();
		// include commit in checksum, but don't include checksum itself
		checksum((char*) commit + sizeof (long), mmf->length(commit) - sizeof (long));
		commit->cksum = cksum;
		cksum = ::checksum(0, 0, 0); // reset
		}
	}

inline bool operator<(const TranRead& tr1, const TranRead& tr2)
	{
	return tr1.tblnum < tr2.tblnum ||
		(tr1.tblnum == tr2.tblnum && strcmp(tr1.index, tr2.index) < 0);
	}

bool Database::refresh(int tran)
	{
	Transaction* t = ck_get_tran(tran);
	if (t->type == READONLY)
		return true;
	if (t->conflict)
		return false;
	validate_reads(t);
	t->asof = t->conflict ? FUTURE : clock++;
	verify(finalize());
	return ! t->conflict;
	}

bool Database::validate_reads(Transaction* t)
	{
	// look at final transactions after asof
	// TODO: maybe track & use read start time instead of transaction
	set<Transaction>::iterator
		begin = lower_bound(final.begin(), final.end(), t->asof);
	sort(t->reads.begin(), t->reads.end());
	TblNum cur_tblnum = -1;
	const char* cur_index = 0;
	short* colnums = 0;
	int nidxcols = 0;
	for (deque<TranRead>::iterator tr = t->reads.begin(); tr != t->reads.end(); ++tr)
		{
		if (tr->tblnum != cur_tblnum || tr->index != cur_index)
			{
			cur_tblnum = tr->tblnum;
			cur_index = tr->index;

			Tbl* tbl = get_table(tr->tblnum);
			if (! tbl)
				continue ;
			colnums = comma_to_nums(tbl->cols, tr->index);
			verify(colnums);

			nidxcols = 1 + count((const char*) tr->index, (const char*) strchr(tr->index, 0), ',');
			}
		// crude removal of record address from org & end
		Record from = tr->org;
		if (from.size() > nidxcols)
			{ from = from.dup(); from.truncate(nidxcols); }
		Record to = tr->end;
		if (to.size() > nidxcols)
			{ to = to.dup(); to.truncate(nidxcols); }

		for (set<Transaction>::iterator iter = begin; iter != final.end(); ++iter)
			{
			for (deque<TranAct>::const_iterator act = iter->acts.begin(); act != iter->acts.end(); ++act)
				{
				if (act->tblnum != tr->tblnum)
					continue ;
				Record rec(input(act->off));
				Record key = project(rec, colnums);
				if (from <= key && key <= to)
					{
					t->conflict = read_conflict(&*iter, tr->tblnum,
							from, to, cur_index, key, act->type);
					return false;
					}
				}
			}
		}
	return true;
	}

char* Database::read_conflict(const Transaction* t, int tblnum, const Record& from,
	const Record& to, const char* index, const Record& key, ActType type)
	{
	OstreamStr os;
	os << "read conflict with" ;
	if (t->session_id && *t->session_id)
		os << ' ' << t->session_id;
	os << " transaction " << t->tran;
	if (Tbl* tbl = get_table(tblnum))
		os << ' ' << tbl->name;
	else
		os << " table " << tblnum;
	os << "^" << index << " from " << from << " to " << to << " key " << key <<
		(type == CREATE_ACT ? " (create)" : " (delete)");
	return os.str();
	}

TranTime Database::table_create_time(TblNum tblnum)
	{
	TranTime* p = table_created.find(tblnum);
	return p ? *p : PAST;
	}

void Database::abort(int tran)
	{
	Transaction* t = get_tran(tran);
	if (! t)
		return ;
	if (t->type == READWRITE)
		{
		for (deque<TranAct>::iterator act = t->acts.begin(); act != t->acts.end(); ++act)
			{
			switch (act->type)
				{
			case CREATE_ACT :
				{
				TranTime* p = created.find(act->off);
				verify(p && *p > UNCOMMITTED);
				created.erase(act->off);
				if (act->time > table_create_time(act->tblnum))
					if (Tbl* tbl = get_table(act->tblnum))
						{
						Record r(input(act->off));
						remove_index_entries(tbl, r);
						// undo tables record update
						--tbl->nrecords;
						tbl->totalsize -= r.cursize();
						tbl->update();
						}
				break ;
				}
			case DELETE_ACT :
				{
				TranDelete* p = deleted.find(act->off);
				verify(p && p->time > UNCOMMITTED);
				deleted.erase(act->off);
				if (Tbl* tbl = get_table(act->tblnum))
					{
					// undo tables record update
					++tbl->nrecords;
					tbl->totalsize += input(act->off).cursize();
					tbl->update();
					}
				break ;
				}
			default :
				unreachable();
				}
			}
		}
	verify(trans.erase(tran));
	verify(finalize());
	}

bool lt_asof(const pair<int,Transaction>& x, const pair<int,Transaction>& y)
	{
	return x.second.asof < y.second.asof;
	}

bool Database::finalize()
	{
	bool ok = true;
	map<int,Transaction>::const_iterator oldest_t =
		min_element(trans.begin(), trans.end(), lt_asof);
	TranTime oldest = (oldest_t == trans.end() ? FUTURE : oldest_t->second.asof);
	while (! final.empty() && final.begin()->asof < oldest)
		{
		const Transaction& t = *final.begin();
		for (deque<TranAct>::const_iterator act = t.acts.begin(); act != t.acts.end(); ++act)
			{
			try
				{
				if (act->type == CREATE_ACT)
					verify(created.erase(act->off));
				else // DELETE_ACT
					{
					verify(deleted.erase(act->off));
					if (act->time > table_create_time(act->tblnum) &&
						get_table(act->tblnum))
						{
						Record r(input(act->off));
						remove_index_entries(get_table(act->tblnum), r);
						}
					}
				}
			catch (const Except&)
				{ ok = false; }
			}
		final.erase(final.begin());
		}
	return ok;
	}

// visibility =======================================================

TranTime Database::create_time(Mmoffset off)
	{
	TranTime* p = created.find(off);
	return p ? *p : PAST;
	}

TranTime Database::delete_time(Mmoffset off)
	{
	TranDelete* p = deleted.find(off);
	return p ? p->time : FUTURE;
	}

bool Database::visible(int tran, Mmoffset address)
	{
	if (tran == schema_tran)
		return true;

	TranTime asof = ck_get_tran(tran)->asof;

	TranTime ct = create_time(address);
	if (ct > UNCOMMITTED)
		{
		if (ct - UNCOMMITTED != tran)
			return false;
		}
	else if (ct > asof)
		return false;

	TranTime dt = delete_time(address);
	if (dt > UNCOMMITTED)
		return dt - UNCOMMITTED != tran;
	return dt >= asof;
	}

void Database::shutdown()
	{
	// abort all outstanding transactions
	for (map<int,Transaction>::iterator i = trans.begin(); i != trans.end(); )
		{
		int tran = i->first;
		++i; // since abort will delete & invalidate iterator position
		abort(tran);
		}
	verify(trans.empty());

	if (! nil(schema_deletes) || cksum != ::checksum(0,0,0))
		{
		write_commit_record(clock++, deque<TranAct>(), 0, 0);
		}

	if (! final.empty())
		errlog("final not empty after shutdown");
	if (! created.empty())
		errlog("created not empty after shutdown");
	if (! deleted.empty())
		errlog("deleted not empty after shutdown");
	if (! nil(schema_deletes))
		errlog("schema_deletes not empty on shutdown");

	new (adr(alloc(sizeof (Session), MM_SESSION))) Session(Session::SHUTDOWN);
	}

void Database::checksum(void* buf, size_t n)
	{
	cksum = ::checksum(cksum, buf, n);
	}

// tests ============================================================

#include "testing.h"
#include "tempdb.h"

#define BEGIN \
	{ TempDB tempdb;
#define SETUP \
	BEGIN \
	setup();
#define END \
	verify(thedb->final.empty()); \
	verify(thedb->trans.empty()); \
	}

class test_transaction : public Tests
	{
	TEST(0, basics)
		{
		BEGIN
		int i = thedb->transaction(READONLY);
		verify(thedb->transaction(READWRITE) == i + 1);
		verify(thedb->trans[i].type == READONLY);
		verify(thedb->trans[i].asof == i);
		verify(thedb->trans[i + 1].type == READWRITE);
		verify(thedb->trans[i + 1].asof == i + 1);
		verify(thedb->commit(i + 1));
		thedb->abort(i);
		END
		}
	TEST(1, finalization)
		{
		SETUP
		int t0 = thedb->transaction(READWRITE);
		thedb->add_record(t0, "test", record("ann"));
		thedb->commit(t0);
		verify(thedb->final.empty());

		int t1 = thedb->transaction(READONLY);
			assert_eq(thedb->trans.begin()->first, t1);

		int t2 = thedb->transaction(READWRITE);
		thedb->add_record(t2, "test", record("joe"));
		int t4 = thedb->transaction(READWRITE);
		thedb->add_record(t4, "test", record("sue"));

		thedb->commit(t2);
			assert_eq(thedb->final.size(), 1);
			assert_eq(thedb->final.begin()->tran, t2);

		int t3 = thedb->transaction(READONLY);
			assert_eq(thedb->trans.begin()->first, t1);

		thedb->commit(t4);
			assert_eq(thedb->final.size(), 2);
			assert_eq(thedb->final.begin()->tran, t2);

		int t5 = thedb->transaction(READONLY);
			assert_eq(thedb->trans.begin()->first, t1);
		thedb->commit(t5); // shouldn't finalize anything
			assert_eq(thedb->final.size(), 2);
		thedb->commit(t1); // should finalize t2
			assert_eq(thedb->trans.begin()->first, t3);
			assert_eq(thedb->final.size(), 1);
			assert_eq(thedb->final.begin()->tran, t4);
		thedb->commit(t3); // should finalize t4
		END
		}
	TEST(3, visibility)
		{
		SETUP

		int t1 = thedb->transaction(READWRITE);
		thedb->update_record(t1, "test", "name", key("fred"), record("joe"));

		int t2 = thedb->transaction(READONLY);
		Record r = thedb->get_index("test", "name")->begin(t2)->key;
		verify(r.getstr(0) == "fred"); // old value, not uncommitted value

		verify(thedb->commit(t1));

		int t3 = thedb->transaction(READONLY);
		r = thedb->get_index("test", "name")->begin(t3)->key;
		verify(r.getstr(0) == "joe"); // new value

		int t4 = thedb->transaction(READWRITE);
		thedb->remove_record(t4, "test", "name", key("joe"));
		verify(thedb->commit(t4));

		r = thedb->get_index("test", "name")->begin(t3)->key;
		verify(r.getstr(0) == "joe"); // new value
		verify(thedb->commit(t3));

		int t5 = thedb->transaction(READONLY);
		verify(thedb->get_index("test", "name")->begin(t5).eof()); // gone
		verify(thedb->commit(t5));

		r = thedb->get_index("test", "name")->begin(t2)->key;
		verify(r.getstr(0) == "fred"); // still old value, operating as-of start time
		verify(thedb->commit(t2));

		END
		}
	TEST(4, reads)
		{
		SETUP

		{ int t = thedb->transaction(READONLY);
		thedb->get_index("test", "name")->begin(t);
		verify(thedb->trans[t].reads.empty());
		thedb->commit(t); }

		{ int t = thedb->transaction(READWRITE);
		thedb->get_index("test", "name")->begin(t, key("a"), key("z"));
		Transaction& tran = thedb->trans[t];
		verify(tran.reads.size() == 1);
		TranRead& trd = tran.reads.back();
		assert_eq(trd.org, key("a"));
		verify(trd.end.hasprefix(key("fred")));
		thedb->commit(t); }

		{ int t = thedb->transaction(READWRITE);
		Index::iterator iter = thedb->get_index("test", "name")->iter(t, key("a"), key("z"));
		--iter;
		Transaction& tran = thedb->trans[t];
		verify(tran.reads.size() == 1);
		TranRead& trd = tran.reads.back();
		assert_eq(trd.end, key("z"));
		verify(trd.org.hasprefix(key("fred")));
		thedb->commit(t); }

		END
		}
	TEST(5, conflicts)
		{
		// conflicting deletes (or updates)
		SETUP
		int t1 = thedb->transaction(READWRITE);
		thedb->remove_record(t1, "test", "name", key("fred"));
		int t2 = thedb->transaction(READWRITE);
		xassert(thedb->remove_record(t2, "test", "name", key("fred")));
		verify(! thedb->commit(t2));
		verify(thedb->commit(t1));
		END

		// conflicting inserts (or updates)
		SETUP
		int t1 = thedb->transaction(READWRITE);
		thedb->add_record(t1, "test", record("joe"));
		int t2 = thedb->transaction(READWRITE);
		thedb->add_record(t2, "test", record("joe"));
		verify(thedb->commit(t2));
		verify(! thedb->commit(t1));
		END

		// read validation problem
		SETUP
		int t = thedb->transaction(READWRITE);
		verify(thedb->get_index("test", "name")->begin(t, key("joe")).eof());
		thedb->add_record(t, "test", record("joe"));
		verify(! thedb->get_index("test", "name")->begin(t, key("joe")).eof());
		verify(thedb->commit(t));
		END
		}
private:
	void setup()
		{
		thedb->add_table("test");
		thedb->add_column("test", "name");
		thedb->add_index("test", "name", true);
		int t = thedb->transaction(READWRITE);
		thedb->add_record(t, "test", record("fred"));
		verify(thedb->commit(t));
		}
	Record record(const char* s)
		{
		Record r;
		r.addval(s);
		return r;
		}
	Record key(const char* s)
		{
		Record r;
		r.addval(s);
		return r;
		}
	};
REGISTER(test_transaction);
