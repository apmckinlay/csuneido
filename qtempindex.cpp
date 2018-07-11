// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "qtempindex.h"
#include "database.h"
#include "thedb.h"

Record row_to_key(const Header& hdr, const Row& row, const Fields& flds)
	{
	Record key;
	for (Fields f = flds; ! nil(f); ++f)
		key.addraw(row.getrawval(hdr, *f));
	if (key.cursize() > 4000)
		except("index entry size > 4000: " << flds << " = " << key);
	return key;
	}

// with a sequence number
Record row_to_key(const Header& hdr, const Row& row, const Fields& flds, int num)
	{
	Record key = row_to_key(hdr, row, flds);
	key.addval(num);
	return key;
	}

// TempIndex1 --------------------------------------------------------

TempIndex1::TempIndex1(Query* s, const Fields& ci, bool u)
	: Query1(s), unique(u), first(true), rewound(true), index(0)
	{
	order = ci;
	}

void TempIndex1::out(Ostream& os) const
	{
	os << *source << " TEMPINDEX1" << order;
	if (unique)
		os << " unique";
	}

Header TempIndex1::header()
	{
	// NOTE: this will have incorrect index as flds[0]
	// but this doesn't matter since our Rows have empty data[0]
	return source->header();
	}

Indexes TempIndex1::indexes()
	{
	return Indexes(order);
	}

void TempIndex1::iterate_setup(Dir dir)
	{
	Header srchdr = source->header();
	TempDest* td = new TempDest;
	index = new VFtree(td);
	Row row;
	for (int num = 0; Eof != (row = source->get(NEXT)); ++num)
		{
		Record key = unique
			? row_to_key(srchdr, row, order)
			: row_to_key(srchdr, row, order, num);
		td->addref(row.data[1].ptr());
		// WARNING: assumes data is always second in row
		verify(index->insert(VFslot(key, row.data[1].to_int64())));
		}
	iter = (dir == NEXT ? index->first() : index->last());
	}

void TempIndex1::select(const Fields& selindex, const Record& from, const Record& to)
	{
	verify(prefix(order, selindex));
	sel.org = from;
	sel.end = to;
	rewound = true;
	}

void TempIndex1::rewind()
	{
	rewound = true;
	}

Row TempIndex1::get(Dir dir)
	{
	if (first)
		{
		first = false;
		hdr = header();
		iterate_setup(dir);
		}
	if (rewound)
		{
		rewound = false;
		if (dir == NEXT)
			iter.seek(sel.org);
		else // dir == PREV
			{
			Record end = sel.end.dup();
			end.addmax();
			iter.seek(end);
			if (iter.eof())
				iter = index->last();
			else
				while (! iter.eof() && iter->key.prefixgt(sel.end))
					--iter;
			}
		}
	else if (dir == NEXT)
		++iter;
	else // dir == PREV
		--iter;
	if (iter.eof())
		{
		rewound = true;
		return Eof;
		}

	// TODO: put iter->key into row
	Row row(lisp(Record(), Record::from_int64(iter->adr, theDB()->mmf)));

	// TODO: should be able to keydup iter->key
	Record key = row_to_key(hdr, row, order);
	if (dir == NEXT)
		{
		key.truncate(sel.end.size());
		if (key > sel.end)
			return Eof;
		}
	else // dir == PREV
		{
		key.truncate(sel.org.size());
		if (key < sel.org)
			return Eof;
		}

	return row;
	}

void TempIndex1::close(Query* q)
	{
	if (index)
		index->free();
	Query1::close(q);
	}

// TempIndexN ---------------------------------------------------------

TempIndexN::TempIndexN(Query* s, const Fields& ci, bool u)
	: Query1(s), unique(u), first(true), rewound(true), index(0)
	{
	order = ci;
	}

void TempIndexN::out(Ostream& os) const
	{
	os << *source << " TEMPINDEXN" << order;
	if (unique)
		os << " unique";
	}

Indexes TempIndexN::indexes()
	{
	return Indexes(unique ? order : concat(order, lisp(gcstring("-"))));
	}

void TempIndexN::iterate_setup(Dir dir)
	{
	TempDest* td = new TempDest;
	index = new VVtree(td);
	Row row;
	for (int num = 0; Eof != (row = source->get(NEXT)); ++num)
		{
		Record key = unique
			? row_to_key(hdr, row, order)
			: row_to_key(hdr, row, order, num);
		Vdata d(row.data);
		for (Lisp<Record> rs = row.data; ! nil(rs); ++rs)
			td->addref(rs->ptr());
		verify(index->insert(VVslot(key, &d)));
		}
	iter = (dir == NEXT ? index->first() : index->last());
	}

void TempIndexN::select(const Fields& selindex, const Record& from, const Record& to)
	{
	verify(prefix(order, selindex));
	sel.org = from;
	sel.end = to;
	rewound = true;
	}

void TempIndexN::rewind()
	{
	rewound = true;
	}

Row TempIndexN::get(Dir dir)
	{
	if (first)
		{
		first = false;
		hdr = source->header();
		iterate_setup(dir);
		}
	if (rewound)
		{
		rewound = false;
		if (dir == NEXT)
			iter.seek(sel.org);
		else // dir == PREV
			{
			Record end = sel.end.dup();
			end.addmax();
			iter.seek(end);
			if (iter.eof())
				iter = index->last();
			else
				while (! iter.eof() && iter->key.prefixgt(sel.end))
					--iter;
			}
		}
	else if (dir == NEXT)
		++iter;
	else // dir == PREV
		--iter;
	if (iter.eof())
		{
		rewound = true;
		return Eof;
		}

	Vdata* d = iter->data;
	Records rs;
	for (int i = d->n - 1; i >= 0; --i)
		rs.push(Record::from_int(d->r[i], theDB()->mmf));
	Row row(rs);

	Record key = row_to_key(hdr, row, order);

	if (dir == NEXT)
		{
		key.truncate(sel.end.size());
		if (key > sel.end)
			return Eof;
		}
	else // dir == PREV
		{
		key.truncate(sel.org.size());
		if (key < sel.org)
			return Eof;
		}

	return row;
	}

void TempIndexN::close(Query* q)
	{
	if (index)
		index->free();
	Query1::close(q);
	}
