#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "btree.h"
#include "slots.h"
#include "mmtypes.h"
#include <limits.h>

extern Record keymin;
extern Record keymax;

struct TranRead;
typedef int TblNum;

struct IndexDest
	{
	explicit IndexDest(Mmfile* m) : mmf(m)
		{ }
	Mmoffset alloc(uint32_t n) const
		{ return mmf->alloc(n, MM_OTHER); }
	void unalloc(uint32_t n) const
		{ mmf->unalloc(n); }
	void* adr(Mmoffset offset) const
		{ return mmf->adr(offset); }

	Mmfile* mmf;
	};

typedef Btree<Vslot,VFslot,Vslots,VFslots,IndexDest> IndexBtree;

class Database;

class Index
	{
public:
	friend class Database;
	typedef Record Key;
	Index(Database* d, TblNum tblnum, const char* idxname,
		bool k, bool u = false);
	Index(Database* d, TblNum tblnum, const char* idxname, Mmoffset r, short tl, int nn,
		bool k, bool u = false);

	bool insert(int tran, Vslot x);
	bool erase(const Key& key)
		{ return bt.erase(key); }
	float rangefrac(const Key& from, const Key& to);

	class iterator
		{
	public:
		friend class Index;
		iterator()
			{ }
		iterator(Index* ix, int tran, Key from, Key to, TranRead* trd);
		void set_transaction(int t)
			{ tran = t; }
		Vslot& operator*()
			{ verify(! rewound); return *iter; }
		Vslot* operator->()
			{ verify(! rewound); return iter.operator->(); }
		Record data();
		void operator++();
		void operator--();
		bool eof()
			{ return iter.eof(); }
		void reset_prevsize()
			{ prevsize = _I64_MAX; }
	private:
		bool visible();

		Index* ix = nullptr;
		IndexBtree::iterator iter;
		Mmoffset prevsize = 0;
		int tran = 0;
		Key from;
		Key to;
		bool rewound = true;
		TranRead* tranread = nullptr;
		};
	friend class iterator;
	iterator begin(int tran);
	iterator begin(int tran, const Key& key);
	iterator begin(int tran, const Key& from, const Key& to);
	iterator iter(int tran)
		{ return iterator(this, tran, keymin, keymax, read_act(tran)); }
	iterator iter(int tran, const Key& key)
		{ return iterator(this, tran, key, key, read_act(tran)); }
	iterator iter(int tran, const Key& from, const Key& to)
		{ return iterator(this, tran, from, to, read_act(tran)); }
	Vslot find(int tran, const Key& key);

	void getinfo(Record& r) const
		{ bt.getinfo(r); }
	int get_nnodes() const
		{ return bt.get_nnodes(); }
	bool is_unique() const
		{ return unique; }
private:
	TranRead* read_act(int tran);

	Database* db;
	IndexBtree bt;
	bool iskey;
	bool unique;
	TblNum tblnum;
	gcstring idxname;
	};

#include "tempdest.h"
typedef Btree<VFslot,VFslot,VFslots,VFslots,TempDest> VFtree;
typedef Btree<VVslot,VFslot,VVslots,VFslots,TempDest> VVtree;
