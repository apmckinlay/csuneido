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

#include "qtable.h"
#include "qselect.h"
#include "database.h"
#include "thedb.h"
#include "fibers.h" // for yield
#include "qhistable.h"
#include "trace.h"

#define LOG(stuff)	TRACE(TABLE, stuff)

Query* Query::make_table(char* s)
	{
	return new Table(s);
	}

Table::Table(char* s) : table(s), first(true),
	rewound(true), tran(INT_MAX), tbl(theDB()->get_table(table))
	{
	if (! theDB()->istable(table))
		except("nonexistent table: " << table);
	singleton = nil(*indexes());
	}

void Table::out(Ostream& os) const
	{
	os << table;
	if (! nil(idx))
		os << "^" << idx;
	}

Fields Table::columns()
	{
	return theDB()->get_columns(table);
	}

Indexes Table::indexes()
	{
	return theDB()->get_indexes(table);
	}

Indexes Table::keys()
	{
	return theDB()->get_keys(table);
	}

double Table::nrecords()
	{
	return theDB()->nrecords(table);
	}

int Table::recordsize()
	{
	int nrecs = theDB()->nrecords(table);
	return nrecs ? theDB()->totalsize(table) / nrecs : 0;
	}

int Table::columnsize()
	{
	return recordsize() / size(columns());
	}

gcstring fields_to_commas(Fields list)
	{
	gcstring s;
	for (; ! nil(list); ++list)
		{
		s += *list;
		if (! nil(cdr(list)))
			s += ",";
		}
	return s;
	}

int Table::keysize(const Fields& index)
	{
	int nrecs = theDB()->nrecords(table);
	if (nrecs == 0)
		return 0;
	Index* idx = theDB()->get_index(table, fields_to_commas(index));
	verify(idx);
	int nnodes = idx->get_nnodes();
	int nodesize = NODESIZE / (nnodes <= 1 ? 4 : 2);
	return (nnodes * nodesize) / nrecs;
	}

int Table::indexsize(const Fields& index)
	{
	Index* idx = theDB()->get_index(table, fields_to_commas(index));
	verify(idx);
	return idx->get_nnodes() * NODESIZE + index.size();
	}

int Table::totalsize()
	{
	return theDB()->totalsize(table);
	}

float Table::iselsize(const Fields& index, const Iselects& iselects)
	{
	Iselects isels;

	LOG("iselsize " << index << " ? " << iselects);

	// first check for matching a known number of records
	if (member(keys(), index) && size(index) == size(iselects))
		{
		int nexact = 1;
		for (isels = iselects; ! nil(isels); ++isels)
			{
			Iselect isel = *isels;
			verify(! isel.none());
			if (isel.type == ISEL_VALUES)
				nexact *= size(isel.values);
			else if (isel.org != isel.end) // range
				{
				nexact = 0;
				break ;
				}
			}
		if (nexact > 0)
			{
			int nrecs = theDB()->nrecords(table);
			return nrecs ? (float) nexact / nrecs : 0;
			// NOTE: assumes they all exist ???
			}
		}

	// TODO: convert this to use Select::selects()

	for (isels = iselects; ! nil(isels); ++isels)
		{
		Iselect& isel = *isels;
		verify(! isel.none());
		if (isel.one())
			{
			}
		else if (isel.type == ISEL_RANGE)
			{
			break ;
			}
		else // set - recurse through values
			{
			Lisp<gcstring> save = isel.values;
			float sum = 0;
			for (Lisp<gcstring> values = isel.values; ! nil(values); ++values)
				{
				isel.values = Lisp<gcstring>(*values);
				sum += iselsize(index, iselects);
				}
			isel.values = save;
			LOG("sum is " << sum);
			return sum;
			}
		}

	// now build the key
	int i = 0;
	Record org;
	Record end;
	for (isels = iselects; ! nil(isels); ++isels, ++i)
		{
		Iselect& isel = *isels;
		verify(! isel.none());
		if (isel.one())
			{
			if (isel.type == ISEL_RANGE)
				{
				org.addraw(isel.org.x);
				end.addraw(isel.org.x);
				}
			else // in set
				{
				org.addraw(*isel.values);
				end.addraw(*isel.values);
				}
			if (nil(cdr(isels)))
				{
				// final exact value (inclusive end)
				++i;
				for (int j = i; j < size(index); ++j)
					end.addmax();
				if (i >= size(index)) // ensure at least one added
					end.addmax();
				}
			}
		else if (isel.type == ISEL_RANGE)
			{
			// final range
			org.addraw(isel.org.x);
			end.addraw(isel.end.x);
			++i;
			if (isel.org.d != 0) // exclusive
				{
				for (int j = i; j < size(index); ++j)
					org.addmax();
				if (i >= size(index)) // ensure at least one added
					org.addmax();
				}
			if (isel.end.d == 0) // inclusive
				{
				for (int j = i; j < size(index); ++j)
					end.addmax();
				if (i >= size(index)) // ensure at least one added
					end.addmax();
				}
			break ;
			}
		else
			unreachable();
		}
	LOG("=\t" << org << " ->\t" << end);
	float frac = theDB()->rangefrac(table, fields_to_commas(index), org, end);
	LOG("frac " << frac);
	return frac;
	}

// find the shortest of indexes with index as a prefix & containing needs
// TODO: use number of nodes instead of number of fields
static Fields match(Indexes idxs, const Fields& index, const Fields& needs)
	{
	Fields best;
	int bestremainder = 9999;
	for (; ! nil(idxs); ++idxs)
		{
		Fields i(*idxs);
		Fields f(index);
		for (; ! nil(i) && ! nil(f) && *i == *f; ++i, ++f)
			;
		if (! nil(f) || ! subset(*idxs, needs))
			continue ;
		int remainder = size(i);
		if (remainder < bestremainder)
			{ best = *idxs; bestremainder = remainder; }
		}
	return best;
	}

int tcn = 0;

const Fields none;

double Table::optimize2(const Fields& index, const Fields& needs,
	const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	if (! subset(columns(), needs))
		except("Table::optimize columns does not contain: " <<
			difference(needs, columns()));
	if (! subset(columns(), index))
		return IMPOSSIBLE;
	++tcn;
	Indexes idxs = indexes();
	if (nil(idxs))
		return IMPOSSIBLE;
	if (singleton)
		{
		idx = nil(index) ? *idxs : index;
		return recordsize();
		}
	double cost1 = IMPOSSIBLE;
	double cost2 = IMPOSSIBLE;
	double cost3 = IMPOSSIBLE;
	Fields idx1, idx2, idx3;
	if (! nil(idx1 = match(idxs, index, needs)))
		// index found that meets all needs
		cost1 = nrecords() * keysize(idx1); // cost of reading index
	if (! nil(firstneeds) && ! nil(idx2 = match(idxs, index, firstneeds)))
		// index found that meets firstneeds
		// assume this means we only have to read 75% of data
		cost2 = .75 * nrecords() * recordsize() + // cost of reading data
			nrecords() * keysize(idx2); // cost of reading index
	if (! nil(needs) && ! nil(idx3 = match(idxs, index, none)))
		cost3 = nrecords() * recordsize() + // cost of reading data
			nrecords() * keysize(idx3); // cost of reading index
	TRACE(TABLE, "optimize " << table << " index " << index <<
		(is_cursor ? " is_cursor" : "") << (freeze ? " FREEZE" : "") <<
		"\n\tneeds: " << needs <<
		"\n\tfirstneeds: " << firstneeds);
	TRACE(TABLE, "\tidx1 " << idx1 << " cost1 " << cost1);
	TRACE(TABLE, "\tidx2 " << idx2 << " cost2 " << cost2);
	TRACE(TABLE, "\tidx3 " << idx3 << " cost3 " << cost3);

	double cost;
	if (cost1 <= cost2 && cost1 <= cost3)
		{ cost = cost1; idx = idx1; }
	else if (cost2 <= cost1 && cost2 <= cost3)
		{ cost = cost2; idx = idx2; }
	else
		{ cost = cost3; idx = idx3; }
	TRACE(TABLE, "\tchose: idx " << idx << " cost " << cost);
	return cost;
	}

// used by Select::optimize
void Table::select_index(const Fields& index)
	{
	idx = index;
	}

Header Table::header()
	{
	// can't use index key as data if it's lower
	Index* i = nil(idx) || singleton ? NULL
		: theDB()->get_index(table, fields_to_commas(idx));
	bool lower = i && i->is_lower();
	return Header(lisp(singleton || lower ? Fields() : idx, 
		theDB()->get_fields(table)), theDB()->get_columns(table));
	}

void Table::set_index(const Fields& index)
	{
	idx = index;
	ix = (nil(idx) || singleton
		? theDB()->first_index(table)
		: theDB()->get_index(table, fields_to_commas(idx)));
	verify(ix);
	rewound = true;
	}

void Table::select(const Fields& index, const Record& from, const Record& to)
	{
	if (! prefix(idx, index))
		except_err(this << " invalid select: " << index << " " << from << " to " << to);
	sel.org = from;
	sel.end = to;
	rewind();
	}

void Table::rewind()
	{
	rewound = true;
	}

void Table::iterate_setup(Dir dir)
	{
	hdr = header();
	ix = (nil(idx) || singleton
		? theDB()->first_index(table)
		: theDB()->get_index(table, fields_to_commas(idx)));
	verify(ix);
	}

// TODO: factor out code common to get's in Table, TempIndex1, TempIndexN
Row Table::get(Dir dir)
	{
	Fibers::yieldif();
	verify(tran != INT_MAX);
	if (first)
		{
		first = false;
		iterate_setup(dir);
		}
	if (rewound)
		{
		rewound = false;
		iter = singleton
			? ix->iter(tran)
			: ix->iter(tran, sel.org, sel.end);
		}
	if (dir == NEXT)
		++iter;
	else // dir == PREV
		--iter;
	if (iter.eof())
		{
		rewound = true;
		return Eof;
		}
	Record r(iter.data());
	if (tbl->num > TN_VIEWS && r.size() > tbl->nextfield)
		except("get: record has more fields (" << r.size() << ") than " << table << " should (" << tbl->nextfield << ")");
	Row row = Row(lisp(iter->key, iter.data()));
	if (singleton)
		{
		Record key = row_to_key(hdr, row, idx);
		if (key < sel.org || sel.end < key)
			{
			rewound = true;
			return Eof;
			}
		}
	return row;
	}

bool Table::output(const Record& r)
	{
	verify(tran != INT_MAX);
	Record r2(r);
	if (tbl->num > TN_VIEWS && r.size() > tbl->nextfield)
		{
		r2 = r.dup();
		r2.truncate(tbl->nextfield);
		}
	theDB()->add_record(tran, table, r2);
	return true;
	}

bool Table::erase(const gcstring& index, const Record& key)
	{
	verify(tran != INT_MAX);
	theDB()->remove_record(tran, table, index, key);
	return true;
	}

bool Table::update(const gcstring& index, const Record& key, const Record& newrec)
	{
	verify(tran != INT_MAX);
	theDB()->update_record(tran, table, index, key, newrec);
	return true;
	}

#include "testing.h"
#include "tempdb.h"

class test_qtable : public Tests
	{
	void adm(char* s)
		{
		except_if(! database_admin(s), "FAILED: " << s);
		}
	int tran;
	void req(char* s)
		{
		except_if(! database_request(tran, s), "FAILED: " << s);
		}

	TEST(0, main)
		{
		TempDB tempdb;

		tran = theDB()->transaction(READWRITE);
		// so trigger searches won't give error
		adm("create stdlib (group, name, text) key(name)");

		adm("create lines (hdrnum, linenum, desc, amount) key(linenum) index(hdrnum)");
		req("insert{hdrnum: 1, linenum: 1, desc: \"now\", amount: 10} into lines");
		req("insert{hdrnum: 1, linenum: 2, desc: \"is\", amount: 20} into lines");
		req("insert{hdrnum: 1, linenum: 3, desc: \"the\", amount: 30} into lines");
		req("insert{hdrnum: 1, linenum: 4, desc: \"time\", amount: 40} into lines");
		req("insert{hdrnum: 1, linenum: 5, desc: \"for\", amount: 50} into lines");
		req("insert{hdrnum: 1, linenum: 6, desc: \"all\", amount: 60} into lines");
		req("insert{hdrnum: 1, linenum: 7, desc: \"good\", amount: 70} into lines");
		req("insert{hdrnum: 2, linenum: 8, desc: \"men\", amount: 80} into lines");
		req("insert{hdrnum: 2, linenum: 9, desc: \"to\", amount: 90} into lines");
		req("insert{hdrnum: 3, linenum: 10, desc: \"come\", amount: 100} into lines");

		Table table("lines");
		SuValue* one = new SuNumber(1);
		gcstring onepacked(one->packsize());
		one->pack(onepacked.buf());
		Iselect oneisel;
		oneisel.org.x = oneisel.end.x = onepacked;
		float f = table.iselsize(lisp(gcstring("linenum")), lisp(oneisel));
		verify(0 < f && f < .2); // should be .1
		f = table.iselsize(lisp(gcstring("hdrnum")), lisp(oneisel));
		verify(.6 < f && f < .8); // should be .7
		}
	};
REGISTER(test_qtable);
