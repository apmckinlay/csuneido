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

#include "index.h"
#include "btree.h"
#include "slots.h"
#include "record.h"
#include "database.h"

// Index ============================================================

Index::Index(Database* d, TblNum t, const char* idx, bool k, bool u)
	: db(d), bt(d->dest), iskey(k), unique(u), tblnum(t), idxname(idx)
	{ }

Index::Index(Database* d, TblNum t, const char* idx, Mmoffset r, short tl, int nn, bool k, bool u)
	: db(d), bt(d->dest, r, tl, nn), iskey(k), unique(u), tblnum(t), idxname(idx)
	{ }

Index::iterator Index::begin(int tran)
	{
	iterator it = iter(tran, keymin, keymax);
	++it;
	return it;
	}

Index::iterator Index::begin(int tran, const Key& key)
	{
	iterator it = iter(tran, key, key);
	++it;
	return it;
	}

Index::iterator Index::begin(int tran, const Key& from, const Key& to)
	{
	iterator it = iter(tran, from, to);
	++it;
	return it;
	}

TranRead* Index::read_act(int tran)
	{
	return db->read_act(tran, tblnum, idxname.str());
	}

static bool empty(Record& key)
	{
	int n = key.size() - 1; // - 1 to ignore record address at end
	if (n == 0)
		return true;
	for (int i = 0; i < n; ++i)
		if (key.getraw(i).size() != 0)
			return false;
	return true;
	}

bool Index::insert(int tran, Vslot x)
	{
	if (iskey || (unique && ! empty(x.key)))
		{
		// strip off record address
		Record& key = x.key;
		key.reuse(key.size() - 1);
		bool dup = ! nil(find(tran, key));
		key.reuse(key.size() + 1);
		if (dup)
			return false;
		}
	return bt.insert(x);
	}

Vslot Index::find(int tran, const Key& key)
	{
	iterator iter = begin(tran, key);
	return ! iter.eof() && iter->key.hasprefix(key) ? *iter : Vslot();
	}

// Index::iterator ==================================================

Index::iterator::iterator(Index* i, int tr, Key f, Key t, TranRead* trd)
	: ix(i), prevsize(_I64_MAX), tran(tr), from(f), to(t), rewound(true), tranread(trd)
	{
	}

Record Index::iterator::data()
	{
	verify(! rewound);
	return ix->db->input(iter->adr());
	}

bool Index::iterator::visible()
	{
	return ix->db->visible(tran, iter->adr());
	}

static bool eq(const Record& r1, const Record& r2)
	{
	const int n = r1.size() - 1;
	if (n != r2.size() - 1)
		return false;
	for (int i = 0; i < n; ++i)
		if (r1.getraw(i) != r2.getraw(i))
			return false;
	return true;
	}

#include "trace.h"

void Index::iterator::operator++()
	{
	bool first = true;
	Record prevkey;
	if (rewound)
		{
		verify(ix);
		iter = ix->bt.locate(from);
		rewound = false;
		if (tranread)
			tranread->org = from;
		}
	else if (! iter.eof())
		{
		prevkey = iter->key;
		first = false;
		++iter;
		}
	while (! iter.eof() &&
		(iter->adr() >= prevsize || ! visible()))
		++iter;
	if (! iter.eof() && iter->key.prefixgt(to))
		iter.seteof();
	if (! iter.eof() && (ix->iskey || first || ! eq(iter->key, prevkey)))
		prevsize = ix->db->mmf->size();
	if (tranread)
		{
		if (iter.eof())
			{
			tranread->end = to;
			if ((trace_level & TRACE_ALLINDEX) && ix->bt.get_nnodes() > 1 &&
				tranread->org == Record() && tranread->end == Record(keymax))
				{
				TRACE(ALLINDEX, ix->db->get_table(ix->tblnum)->name << " " <<
					ix->idxname);
				extern void trace_last_q();
				trace_last_q();
				}
			}
		else if (iter->key > tranread->end)
			tranread->end = iter->key;
		}
	}

void Index::iterator::operator--()
	{
	if (rewound)
		{
		verify(ix);
		Record key = to.dup();
		key.addmax();
		iter = ix->bt.locate(key);
		if (iter.eof())
			iter = ix->bt.last();
		else
			while (! iter.eof() && iter->key.prefixgt(to))
				--iter;
		rewound = false;
		if (tranread)
			tranread->end = to;
		}
	else if (! iter.eof())
		--iter;
	while (! iter.eof() && ! visible())
		--iter;
	prevsize = ix->db->mmf->size();
	if (! iter.eof() && iter->key < from)
		iter.seteof();
	if (tranread)
		{
		if (iter.eof())
			tranread->org = from;
		else if (iter->key < tranread->org)
			tranread->org = iter->key;
		}
	}

static bool sameKey(const Record& from, const Record& to)
	{
	extern gcstring fieldmax;
	if (from.size() != to.size() - 1)
		return false;
	if (to.getraw(to.size() - 1) != fieldmax)
		return false;
	for (int i = 0; i < from.size(); ++i)
		if (from.getraw(i) != to.getraw(i))
			return false;
	return true;
	}

float Index::rangefrac(const Key& from, const Key& to)
	{
	if (iskey && sameKey(from, to))
		{
		int n = db->nrecords(db->get_table(tblnum)->name);
		return n > 0 ? 1.0f / n : .001f;
		}
	return bt.rangefrac(from, to);
	}

#include "testing.h"
#include "random.h"
#include "tempdb.h"

const int keysize = 16;

static char buf[128];

long kk(Record& r)
	{
	return r.getlong(1);
	}
Record k(ulong recnum)
	{
	srand(recnum);
	Record r;

	char* dst = buf;
	short n = random(keysize) + 2;
	for (short i = 0; i < n; ++i)
		*dst++ = 'a' + random(26);
	*dst = 0;
	r.addval(buf);

	r.addval(recnum);
	const Mmoffset offset =
#ifdef BIGDB
		((Mmoffset) 1 << 30);
#else
		1 << 10;
#endif
	r.addmmoffset((recnum << 2) + offset);
	assert_eq((r.getmmoffset(r.size() - 1) - offset), (recnum << 2));

	verify(kk(r) == recnum);
	return r;
	}

static Record record(const char* s)
	{
	Record r;
	r.addval(s);
	return r;
	}
static Record record(const char* s, const char* t)
	{
	Record r;
	r.addval(s);
	r.addval(t);
	return r;
	}
static Record key(const char* s)
	{
	Record r;
	r.addval(s);
	return r;
	}
static Record key(const char* s, const char* t)
	{
	Record r;
	r.addval(s);
	r.addval(t);
	return r;
	}
static Record key1(const Record& r)
	{
	Record k;
	k.addraw(r.getraw(0));
	return k;
	}
static Record key2(const Record& r)
	{
	Record k;
	k.addraw(r.getraw(0));
	k.addraw(r.getraw(1));
	return k;
	}

TEST(index_standalone)
	{
	TempDB tempdb;

	Index f(thedb, 0, "", false);

	const int N = 4000;

	int tran = thedb->transaction(READWRITE);

	//insert
	int i;
	for (i = 0; i < N; ++i)
		{
		verify(f.insert(tran, Vslot(k(i))));
   		}

	//find
	for (i = 0; i < N; ++i)
		{
		verify(! nil(f.find(tran, k(i))));
		}

	//iterate
	{
	int n = 0;
	for (Index::iterator iter = f.begin(tran); ! iter.eof(); ++iter, ++n)
		{
		Record key = iter->key;
		verify(0 <= kk(key) && kk(key) < N);
		}
	assert_eq(n, N);
	}

	//reverse iterate
	{
	int n = 0;
	Index::iterator iter = f.iter(tran);
	for (--iter; ! iter.eof(); --iter, ++n)
		{
		Record key = iter->key;
		verify(0 <= kk(key) && kk(key) < N);
		}
	assert_eq(n, N);
	}

	//erase
	srand(1234);
	for (i = 0; i < N; i += 2)
		{ verify(f.erase(k(i))); rand(); }
	//find
	srand(1234);
	for (i = 0; i < N; i += 2)
		{
		verify(nil(f.find(tran, k(i))));
		verify(! nil(f.find(tran, k(i+1))));
		}
	//erase
	srand(1234);
	for (i = 1; i < N; i += 2)
		{ rand(); verify(f.erase(k(i))); }

	verify(thedb->commit(tran));
	}

TEST(index_single)
	{
	TempDB tempdb;

	thedb->add_table("test");
	thedb->add_column("test", "name");
	thedb->add_index("test", "name", false);

	Record recs[] =
		{
		record("andrew"),
		record("fred"),
		record("fred"),
		record("leeann"),
		record("leeann"),
		record("tracy")
		};
	size_t n = sizeof (recs) / sizeof (Record);
	int i;
	Index::iterator iter;

	int t = thedb->transaction(READWRITE);
	for (i = 0; i < n; ++i)
		thedb->add_record(t, "test", recs[i]);
	verify(thedb->commit(t));

	Index* index = thedb->get_index("test", "name");
	verify(index);

	t = thedb->transaction(READONLY);

	// find
	verify(nil(index->find(t, key(""))));
	verify(nil(index->find(t, key("joe"))));
	verify(nil(index->find(t, key("z"))));
	for (i = 0; i < n; ++i)
		verify(! nil(index->find(t, key1(recs[i]))));

	// iterate range
	for (iter = index->begin(t), i = 0; i < n; ++iter, ++i)
		verify(! iter.eof() && iter->key.hasprefix(key1(recs[i])));
	for (iter = index->begin(t, key("f"), key("m")), i = 1; i <= 4; ++iter, ++i)
		verify(! iter.eof() && iter->key.hasprefix(key1(recs[i])));
	for (iter = index->begin(t, key("fred"), key("leeann")), i = 1; i <= 4; ++iter, ++i)
		verify(! iter.eof() && iter->key.hasprefix(key1(recs[i])));
	for (iter = index->begin(t, key("fred")), i = 1; i <= 2; ++iter, ++i)
		verify(! iter.eof() && iter->key.hasprefix(key1(recs[i])));

	// reverse iterate range
	for (iter = index->iter(t), --iter, i = n-1; i >= 0; --iter, --i)
		verify(! iter.eof() && iter->key.hasprefix(key1(recs[i])));
	for (iter = index->iter(t, key("f"), key("m")), --iter, i = 4; i >= 1; --iter, --i)
		verify(! iter.eof() && iter->key.hasprefix(key1(recs[i])));
	for (iter = index->iter(t, key("fred"), key("leeann")), --iter, i = 4; i >= 1; --iter, --i)
		verify(! iter.eof() && iter->key.hasprefix(key1(recs[i])));
	for (iter = index->iter(t, key("fred")), --iter, i = 2; i >= 1; --iter, --i)
		verify(! iter.eof() && iter->key.hasprefix(key1(recs[i])));

	// empty ranges
	iter = index->begin(t, key("mike"));
	verify(iter.eof());
	iter = index->begin(t, key("m"), key("r"));
	verify(iter.eof());

	verify(thedb->commit(t));
	}

TEST(index_multi)
	{
	TempDB tempdb;

	thedb->add_table("test");
	thedb->add_column("test", "city");
	thedb->add_column("test", "name");
	thedb->add_index("test", "city,name", true);

	Record recs[] =
		{
		record("calgary", "fred"),
		record("calgary", "leeann"),
		record("calgary", "tracy"),
		record("saskatoon", "andrew"),
		record("saskatoon", "fred"),
		record("saskatoon", "leeann"),
		};
	size_t n = sizeof (recs) / sizeof (Record);
	int i;
	Index::iterator iter;

	int t = thedb->transaction(READWRITE);
	for (i = 0; i < n; ++i)
		thedb->add_record(t, "test", recs[i]);
	verify(thedb->commit(t));

	Index* index = thedb->get_index("test", "city,name");
	verify(index);

	t = thedb->transaction(READONLY);

	// find
	verify(nil(index->find(t, key(""))));
	verify(nil(index->find(t, key("montreal"))));
	verify(nil(index->find(t, key("zealand"))));
	verify(nil(index->find(t, key("calgary", "andrew"))));
	verify(nil(index->find(t, key("saskatoon", "tracy"))));
	for (i = 0; i < n; ++i)
		verify(! nil(index->find(t, key2(recs[i]))));

	// iterate range
	for (iter = index->begin(t), i = 0; i < n; ++iter, ++i)
		verify(! iter.eof() && iter->key.hasprefix(key2(recs[i])));
	for (iter = index->begin(t, key("calgary")), i = 0; i <= 2; ++iter, ++i)
		verify(! iter.eof() && iter->key.hasprefix(key2(recs[i])));
	for (iter = index->begin(t, key("saskatoon")), i = 3; i <= 5; ++iter, ++i)
		verify(! iter.eof() && iter->key.hasprefix(key2(recs[i])));
	for (iter = index->begin(t, key("calgary", "leeann"), key("saskatoon", "fred")),
		i = 1; i <= 4; ++iter, ++i)
		verify(! iter.eof() && iter->key.hasprefix(key2(recs[i])));

	// reverse iterate range
	for (iter = index->iter(t), --iter, i = n-1; i >= 0; --iter, --i)
		verify(! iter.eof() && iter->key.hasprefix(key2(recs[i])));
	for (iter = index->iter(t, key("calgary")), --iter, i = 2; i >= 0; --iter, --i)
		verify(! iter.eof() && iter->key.hasprefix(key2(recs[i])));
	for (iter = index->iter(t, key("saskatoon")), --iter, i = 5; i >= 3; --iter, --i)
		verify(! iter.eof() && iter->key.hasprefix(key2(recs[i])));
	for (iter = index->iter(t, key("calgary", "leeann"), key("saskatoon", "fred")),
		--iter, i = 4; i >= 1; --iter, --i)
		verify(! iter.eof() && iter->key.hasprefix(key2(recs[i])));

	verify(thedb->commit(t));
	}
