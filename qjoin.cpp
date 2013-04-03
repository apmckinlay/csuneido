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

#include "qjoin.h"
#include <algorithm>
#include "trace.h"

Query* Query::make_join(Query* s1, Query* s2, Fields by)
	{
	return new Join(s1, s2, by);
	}

Join::Join(Query* s1, Query* s2, Fields by)
	: Query2(s1, s2),  type(NONE), nrecs(-1), first(true), cols1(0), cols2(0)
	{
	joincols = intersect(source->columns(), source2->columns());
	if (nil(joincols))
		except("join: common columns required");
	if (! nil(by) && ! set_eq(by, joincols))
		except("join: by does not match common columns: " << joincols);

	// find out if joincols include keys
	Indexes k1 = source->keys();
	for (; ! nil(k1); ++k1)
		if (subset(joincols, *k1))
			break ;
	Indexes k2 = source2->keys();
	for (; ! nil(k2); ++k2)
		if (subset(joincols, *k2))
			break ;
	// k1 & k2 are nil if joincols doesn't contain any key
	if (! nil(k1) && ! nil(k2))
		type = ONE_ONE;
	else if (! nil(k1))
		type = ONE_N;
	else if (! nil(k2))
		type = N_ONE;
	else
		type = N_N;
	}

static const char* typestr[] = { "", " 1:1", " 1:n", " n:1", " n:n" };
void Join::out(Ostream& os) const
	{
	os << "(" << *source << ") " <<
		name() << typestr[type] << " on " << joincols <<
		" (" << *source2 << ")";
	}

Indexes Join::keys()
	{
	if (type == ONE_ONE)
		return set_union(source->keys(), source2->keys());
	else if (type == ONE_N)
		return source2->keys();
	else if (type == N_ONE)
		return source->keys();
	else // N_N
		return keypairs();
	}

Indexes Join::keypairs() const
	{
	Indexes keys;
	for (Indexes k1 = source->keys(); ! nil(k1); ++k1)
		{
		Indexes k2 = source2->keys();
		for (; ! nil(k2); ++k2)
			keys.push(set_union(*k1, *k2));
		}
	verify(! nil(keys));
	// TODO: eliminate duplicates
	return keys;
	}

Indexes Join::indexes()
	{
	if (type == ONE_ONE)
		return set_union(source->indexes(), source2->indexes());
	else if (type == ONE_N)
		return source2->indexes();
	else if (type == N_ONE)
		return source->indexes();
	else // n:n
		{
		// union of indexes that don't include joincols
		Indexes idxs;
		Indexes i;
		for (i = source->indexes(); ! nil(i); ++i)
			if (nil(intersect(*i, joincols)))
				idxs.push(*i);
		for (i = source2->indexes(); ! nil(i); ++i)
			if (nil(intersect(*i, joincols)) && ! member(idxs, *i))
				idxs.push(*i);
		return idxs.reverse();
		}
	}

double Join::nrecords()
	{
	verify(nrecs >= 0);
	return nrecs;
	}

int Join::recordsize()
	{
	return source->recordsize() + source2->recordsize();
	}

Lisp<Fixed> Join::fixed() const
	{
	return set_union(source->fixed(), source2->fixed());
	}

double Join::optimize2(const Fields& index, const Fields& needs, const Fields& /*firstneeds*/, bool is_cursor, bool freeze)
	{
	Fields needs1 = intersect(source->columns(), needs);
	Fields needs2 = intersect(source2->columns(), needs);
	verify(size(set_union(needs1, needs2)) == size(needs));

	double cost1 = opt(source, source2, type, index, needs1, needs2, is_cursor);
	double cost2 = can_swap()
		? opt(source2, source, reverse(type), index, needs2, needs1, is_cursor) + OUT_OF_ORDER
		: IMPOSSIBLE;
	double cost = min(cost1, cost2);
	if (cost >= IMPOSSIBLE)
		return IMPOSSIBLE;
	if (freeze)
		{
		if (cost2 < cost1)
			{
			std::swap(source, source2);
			std::swap(needs1, needs2);
			type = reverse(type);
			}
		opt(source, source2, type, index, needs1, needs2, is_cursor, true);
		}
	return cost;
	}

double Join::opt(Query* src1, Query* src2, Type type,
	const Fields& index, const Fields& needs1, const Fields& needs2, bool is_cursor, bool freeze)
	{
	TRACE(JOINOPT, "JOIN " << typestr[type] << " =====================");
	TRACE(JOINOPT, "SRC1 " << src1);
	TRACE(JOINOPT, "SRC2 " << src2);
	// SELECT_COST needs to be high to discourage N to 1 when N is large
	const double SELECT_COST = 1000;

	// always have to read all of source 1
	double cost1 = src1->optimize(index, needs1, joincols, is_cursor, freeze);
	if (cost1 >= IMPOSSIBLE)
		return IMPOSSIBLE;
	double nrecs1 = src1->nrecords();
	TRACE(JOINOPT, "nrecs1 " << nrecs1 << " cost1 = " << cost1 << " + " << 
		(nrecs1 * SELECT_COST) << " = " << (cost1 + nrecs1 * SELECT_COST));

	// for each of source 1, select on source2
	cost1 += nrecs1 * SELECT_COST;

	// cost of reading all of source 2
	double cost2 = src2->optimize(joincols, needs2, Fields(), is_cursor, false);
	if (cost2 >= IMPOSSIBLE)
		return IMPOSSIBLE;
	double nrecs2 = src2->nrecords();
	bool is_cursor2 = is_cursor;
	TRACE(JOINOPT, "nrecs2 " << nrecs2 << " cost2 " << cost2);

	if ((type == N_ONE || type == ONE_ONE) && nrecs1 >= 0 && nrecs2 > 0)
		{
		// can't read any more records from right side than left side
		// if right side is bigger, try passing is_cursor = true to avoid temp indexes
		double p = nrecs1 / nrecs2;
		if (! is_cursor && p < 1)
			{
			double cost2b = src2->optimize(joincols, needs2, Fields(), true, false);
			if (cost2b * p  < cost2)
				{
				is_cursor2 = true;
				cost2 = cost2b;
				TRACE(JOINOPT, "ADJUST cost2 = " << cost2);
				}
			}
		if (p < 1 && is_cursor2)
			{
			TRACE(JOINOPT, "ADJUST cost2 *= " << p << " = " << (cost2 * p));
			cost2 *= p;
			}
		}
	if (freeze)
		src2->optimize(joincols, needs2, Fields(), is_cursor2, true);

	switch (type)
		{
	case ONE_ONE :
		nrecs = min(nrecs1, nrecs2);
		break ;
	case N_ONE :
		nrecs = nrecs2 <= 0 ? 0 : nrecs1;
		break ;
	case ONE_N :
		nrecs = nrecs1 <= 0 ? 0 : nrecs2;
		break ;
	case N_N :
		nrecs = nrecs1 * nrecs2;
		break ;
	default :
		unreachable();
		}
	TRACE(JOINOPT, "nrecs = " << nrecs << " / 2 = " << (nrecs > 1 ? nrecs / 2 : nrecs));
	if (nrecs > 1)
		nrecs /= 2; // convert from max to guess of expected

	if (nrecs <= 0)
		cost2 = 0;
	TRACE(JOINOPT, "cost = " << cost1 << " + " << cost2 << " = " << 
		(cost1 + cost2));

	return cost1 + cost2;
	}

// execution

Header Join::header()
	{
	return source->header() + source2->header();
	}

Row Join::get(Dir dir)
	{
	if (first)
		{
		first = false;
		hdr1 = source->header();
		row2 = Eof;
		empty2 = Row(lispn(Record(), source2->header().size()));
		}
	while (true)
		{
		if (row2 == Eof && ! next_row1(dir))
			return Eof;
		row2 = source2->get(dir);
		if (should_output(row2))
			{
#ifndef NDEBUG
			if (row2 != Eof)
				asserteq(row_to_key(hdr1, row1, joincols), row_to_key(source2->header(), row2, joincols));
#endif
			return row1 + (row2 == Eof ? empty2 : row2);
			}
		}
	}

bool Join::next_row1(Dir dir)
	{
	if (Eof == (row1 = source->get(dir)))
		return false;
	Record key = row_to_key(hdr1, row1, joincols);
	source2->select(joincols, key);
	return true;
	}

bool Join::should_output(const Row& row)
	{
	return row != Eof;
	}

void Join::select(const Fields& index, const Record& from, const Record& to)
	{
	first = true;
	source->select(index, from, to);
	}

void Join::rewind()
	{
	first = true;
	source->rewind();
	}

// LeftJoin =========================================================

Query* Query::make_leftjoin(Query* s1, Query* s2, Fields by)
	{
	return new LeftJoin(s1, s2, by);
	}

LeftJoin::LeftJoin(Query* s1, Query* s2, Fields by) : Join(s1, s2, by)
	{ }

Indexes LeftJoin::keys()
	{
	if (type == ONE_ONE || type == N_ONE)
		return source->keys();
	else // ONE_N or N_N
		return keypairs();
	}

double LeftJoin::nrecords()
	{
	verify(nrecs >= 0);
	return source->nrecords();
	}

bool LeftJoin::next_row1(Dir dir)
	{
	row1_output = false;
	return Join::next_row1(dir);
	}

bool LeftJoin::should_output(const Row& row)
	{
	if (! row1_output)
		{
		row1_output = true;
		return true;
		}
	return Join::should_output(row);
	}
