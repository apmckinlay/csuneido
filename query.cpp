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

#include "queryimp.h"
#include "qtempindex.h"
#include "qsort.h"
#include "database.h"
#include "commalist.h"
#include "trace.h"
#include "ostreamstr.h"
#include "exceptimp.h"

Row Query::Eof;

const Fields none;

Query* query(char* s, bool is_cursor)
	{
	return query_setup(parse_query(s), is_cursor);
	}

Query* query_setup(Query* q, bool is_cursor)
	{
	q = q->transform();
	if (q->optimize(Fields(), q->columns(), Fields(), is_cursor, true) >= IMPOSSIBLE)
		except("invalid query");
	q = q->addindex();
	trace_tempindex(q);
	return q;
	}

static bool hasTempIndex(Query* q)
	{
	if (dynamic_cast<TempIndex1*>(q) || dynamic_cast<TempIndexN*>(q))
		return true;
	if (Query2* q2 = dynamic_cast<Query2*>(q))
		return hasTempIndex(q2->source) || hasTempIndex(q2->source2);
	if (Query1* q1 = dynamic_cast<Query1*>(q))
		return hasTempIndex(q1->source);
	return false;
	}

void trace_tempindex(Query* q)
	{
	if ((trace_level & TRACE_TEMPINDEX) && hasTempIndex(q))
		TRACE(TEMPINDEX, q);
	}

Ostream& operator<<(Ostream& os, const Query& node)
	{
	node.out(os);
	return os;
	}

Query::Query()
	{
	}

bool Query::output(const Record&)
	{
	except("output is not allowed on this query:\n" << this);
	}

Query* Query::addindex()
	{
	if (nil(tempindex))
		return this;

	Indexes k;
	for (k = keys(); ! nil(k); ++k)
		if (subset(tempindex, *k))
			break ;
	bool unique = ! nil(k);

	if (header().size() > 2)
		return new TempIndexN(this, tempindex, unique);
	else
		return new TempIndex1(this, tempindex, unique);
	}

// find the cheapest key index
Fields Query::key_index(const Fields& needs)
	{
	Fields best_index;
	double best_cost = IMPOSSIBLE;
	for (Indexes ks = keys(); ! nil(ks); ++ks)
		{
		double cost = optimize(*ks, needs, Fields(), false, false);
		if (cost < best_cost)
			{ best_cost = cost; best_index = *ks; }
		}
	return best_index;
	}

// tempindex ?
double Query::optimize(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	TRACE(QUERYOPT, "Query::optimize START " << this << (freeze ? " FREEZE" : "") << endl <<
		"\tindex: " << index << (is_cursor ? " is_cursor" : "") <<
		" needs: " << needs << " firstneeds: " << firstneeds);
	if (is_cursor || nil(index))
		{
		double cost = optimize1(index, needs, firstneeds, is_cursor, freeze);
		TRACE(QUERYOPT, "Query::optimize END " << this << endl <<
			"\tcost " << cost << endl);
		return cost;
		}
	if (! subset(columns(), index))
		return IMPOSSIBLE;

	// use existing index
	double cost1 = optimize1(index, needs, firstneeds, is_cursor, false);

	// tempindex
	double cost2 = IMPOSSIBLE;
	int keysize = size(index) * columnsize() * 2; // *2 for index overhead
	Fields tempindex_needs = set_union(needs, index);	
	double no_index_cost = optimize1(none, tempindex_needs, firstneeds, is_cursor, false);
	double tempindex_cost =
		nrecords() * keysize * WRITE_FACTOR	// write index
		+ nrecords() * keysize					// read index
		+ 4000;									// minimum fixed cost
	cost2 = no_index_cost + tempindex_cost;
	verify(cost2 >= 0);

	TRACE(QUERYOPT, "Query::optimize END " << this << endl <<
		"\twith " << index << " cost " << cost1 << endl <<
		"\twith TEMPINDEX cost " << cost2 << "(" << no_index_cost << " + " << tempindex_cost << ") " <<
			" nrecords " << nrecords() << " keysize " << keysize << endl);

	double cost = min(cost1, cost2);
	if (cost >= IMPOSSIBLE)
		return IMPOSSIBLE;
	if (freeze)
		{
		if (cost1 <= cost2)
			optimize1(index, needs, firstneeds, is_cursor, true);
		else // cost2 < cost1
			{
			tempindex = index;
			optimize1(none, tempindex_needs, firstneeds, is_cursor, true);
			}
		}
	return cost;
	}

// cache
double Query::optimize1(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	double cost;
	if (! freeze && 0 <= (cost = cache.get(index, needs, firstneeds, is_cursor)))
		return cost;

	cost = optimize2(index, needs, firstneeds, is_cursor, freeze);
	verify(cost >= 0);

	if (! freeze)
		cache.add(index, needs, firstneeds, is_cursor, cost);
	return cost;
	}

void Query::select(const Fields& index, const Record& key)
	{
	Record key_to = key.dup();
	key_to.addmax();
	select(index, key, key_to);
	}

// Query1 ===========================================================

bool isfixed(Lisp<Fixed> f, const gcstring& field)
	{
	for (; ! nil(f); ++f)
		if (f->field == field && f->values.size() == 1)
			return true;
	return false;
	}

bool prefixed(Fields index, Fields order, const Lisp<Fixed>& fixed)
	{
	while (! nil(index) && ! nil(order))
		{
		if (*index == *order)
			++order, ++index;
		else if (isfixed(fixed, *index))
			++index;
		else if (isfixed(fixed, *order))
			++order;
		else
			return false;
		}
	while (! nil(order) && isfixed(fixed, *order))
		++order;
	return nil(order);
	}

void Query1::best_prefixed(Indexes idxs, const Fields& by,
	const Fields& needs, bool is_cursor,
	Fields& best_index, double& best_cost)
	{
	Lisp<Fixed> fixed = source->fixed();
	for (; ! nil(idxs); ++idxs)
		{
		Fields ix = *idxs;
		if (prefixed(ix, by, fixed))
			{
			// NOTE: optimize1 to bypass tempindex
			double cost = source->optimize1(ix, needs, Fields(), is_cursor, false);
			if (cost < best_cost)
				{ best_cost = cost; best_index = ix; }
			}
		}
	}

// tests ============================================================

#include "testing.h"
#include "suvalue.h"
#include "ostreamstr.h"
#include "thedb.h"

struct Querystruct
	{
	char* query;
	char* explain;
	char* result;
	};

Querystruct querytests[] =
	{
		{"customer", "customer^(id)", // 0
"id	name	city\n\
\"a\"	\"axon\"	\"saskatoon\"\n\
\"c\"	\"calac\"	\"calgary\"\n\
\"e\"	\"emerald\"	\"vancouver\"\n\
\"i\"	\"intercon\"	\"saskatoon\"\n"},

		{"inven", "inven^(item)", // 1
"item	qty\n\
\"disk\"	5\n\
\"mouse\"	2\n\
\"pencil\"	7\n" },

		{"trans", "trans^(item)", // 2
"item	id	cost	date\n\
\"disk\"	\"a\"	100	970101\n\
\"eraser\"	\"c\"	150	970201\n\
\"mouse\"	\"e\"	200	960204\n\
\"mouse\"	\"c\"	200	970101\n" },

		{ "hist", "hist^(date)", // 3
"date	item	id	cost\n\
970101	\"disk\"	\"a\"	100\n\
970101	\"disk\"	\"e\"	200\n\
970102	\"mouse\"	\"c\"	200\n\
970103	\"pencil\"	\"e\"	300\n" },

		{ "hist where item =~ 'e'",
		"hist^(date,item,id) WHERE^(date,item,id) (item =~ \"e\")", // 4
"date	item	id	cost\n\
970102	\"mouse\"	\"c\"	200\n\
970103	\"pencil\"	\"e\"	300\n" },

		{ "hist where item =~ 'e' where item =~ 'e'",
		"hist^(date,item,id) WHERE^(date,item,id) ((item =~ \"e\") and (item =~ \"e\"))", // 5
"date	item	id	cost\n\
970102	\"mouse\"	\"c\"	200\n\
970103	\"pencil\"	\"e\"	300\n" },

		{ "hist where item =~ 'e' where item =~ 'e' where item =~ 'e'",
		"hist^(date,item,id) WHERE^(date,item,id) ((item =~ \"e\") and (item =~ \"e\") and (item =~ \"e\"))", // 6
"date	item	id	cost\n\
970102	\"mouse\"	\"c\"	200\n\
970103	\"pencil\"	\"e\"	300\n" },

		{ "alias", "alias^(id)",
"id	name2\n\
\"a\"	\"abc\"\n\
\"c\"	\"trical\"\n" },

		{ "supplier", "supplier^(city)",
"supplier	name	city\n\
\"mec\"	\"mtnequipcoop\"	\"calgary\"\n\
\"hobo\"	\"hoboshop\"	\"saskatoon\"\n\
\"ebs\"	\"ebssail&sport\"	\"saskatoon\"\n\
\"taiga\"	\"taigaworks\"	\"vancouver\"\n" },

		{ "customer project id,name", "customer^(id) PROJECT-COPY (id,name)",
"id	name\n\
\"a\"	\"axon\"\n\
\"c\"	\"calac\"\n\
\"e\"	\"emerald\"\n\
\"i\"	\"intercon\"\n" },

		{ "trans project item", "trans^(item) PROJECT-SEQ^(item) (item)",
"item\n\
\"disk\"\n\
\"eraser\"\n\
\"mouse\"\n" },

		{ "trans project item,id,cost,date project item", "trans^(item) PROJECT-SEQ^(item) (item)",
"item\n\
\"disk\"\n\
\"eraser\"\n\
\"mouse\"\n" },

		{ "trans project item,id,cost project item,id project item", "trans^(item) PROJECT-SEQ^(item) (item)",
"item\n\
\"disk\"\n\
\"eraser\"\n\
\"mouse\"\n" },

		{ "hist project date,item", "hist^(date,item,id) PROJECT-SEQ^(date,item,id) (date,item)",
"date	item\n\
970101	\"disk\"\n\
970102	\"mouse\"\n\
970103	\"pencil\"\n" },

		{ "customer project city", "customer^(id) PROJECT-LOOKUP (city)",
"city\n\
\"saskatoon\"\n\
\"calgary\"\n\
\"vancouver\"\n"},

		{ "customer project id,city project city", "customer^(id) PROJECT-LOOKUP (city)",
"city\n\
\"saskatoon\"\n\
\"calgary\"\n\
\"vancouver\"\n"},

		{ "customer rename city to location", "customer^(id) RENAME city to location",
"id	name	location\n\
\"a\"	\"axon\"	\"saskatoon\"\n\
\"c\"	\"calac\"	\"calgary\"\n\
\"e\"	\"emerald\"	\"vancouver\"\n\
\"i\"	\"intercon\"	\"saskatoon\"\n" },

		{ "customer rename city to location rename location to loc", "customer^(id) RENAME city to loc",
"id	name	loc\n\
\"a\"	\"axon\"	\"saskatoon\"\n\
\"c\"	\"calac\"	\"calgary\"\n\
\"e\"	\"emerald\"	\"vancouver\"\n\
\"i\"	\"intercon\"	\"saskatoon\"\n" },

		{ "customer rename id to i rename name to n rename city to c", "customer^(id) RENAME id to i, name to n, city to c",
"i	n	c\n\
\"a\"	\"axon\"	\"saskatoon\"\n\
\"c\"	\"calac\"	\"calgary\"\n\
\"e\"	\"emerald\"	\"vancouver\"\n\
\"i\"	\"intercon\"	\"saskatoon\"\n" },

	{ "inven rename item to part, qty to onhand", "inven^(item) RENAME item to part, qty to onhand",
"part	onhand\n\
\"disk\"	5\n\
\"mouse\"	2\n\
\"pencil\"	7\n" },

	{ "inven rename item to part rename qty to onhand", "inven^(item) RENAME item to part, qty to onhand",
"part	onhand\n\
\"disk\"	5\n\
\"mouse\"	2\n\
\"pencil\"	7\n" },

	{ "customer times inven", "(customer^(id)) TIMES (inven^(item))",
"id	name	city	item	qty\n\
\"a\"	\"axon\"	\"saskatoon\"	\"disk\"	5\n\
\"a\"	\"axon\"	\"saskatoon\"	\"mouse\"	2\n\
\"a\"	\"axon\"	\"saskatoon\"	\"pencil\"	7\n\
\"c\"	\"calac\"	\"calgary\"	\"disk\"	5\n\
\"c\"	\"calac\"	\"calgary\"	\"mouse\"	2\n\
\"c\"	\"calac\"	\"calgary\"	\"pencil\"	7\n\
\"e\"	\"emerald\"	\"vancouver\"	\"disk\"	5\n\
\"e\"	\"emerald\"	\"vancouver\"	\"mouse\"	2\n\
\"e\"	\"emerald\"	\"vancouver\"	\"pencil\"	7\n\
\"i\"	\"intercon\"	\"saskatoon\"	\"disk\"	5\n\
\"i\"	\"intercon\"	\"saskatoon\"	\"mouse\"	2\n\
\"i\"	\"intercon\"	\"saskatoon\"	\"pencil\"	7\n" },

	{ "inven times customer", "(inven^(item)) TIMES (customer^(id))",
"item	qty	id	name	city\n\
\"disk\"	5	\"a\"	\"axon\"	\"saskatoon\"\n\
\"disk\"	5	\"c\"	\"calac\"	\"calgary\"\n\
\"disk\"	5	\"e\"	\"emerald\"	\"vancouver\"\n\
\"disk\"	5	\"i\"	\"intercon\"	\"saskatoon\"\n\
\"mouse\"	2	\"a\"	\"axon\"	\"saskatoon\"\n\
\"mouse\"	2	\"c\"	\"calac\"	\"calgary\"\n\
\"mouse\"	2	\"e\"	\"emerald\"	\"vancouver\"\n\
\"mouse\"	2	\"i\"	\"intercon\"	\"saskatoon\"\n\
\"pencil\"	7	\"a\"	\"axon\"	\"saskatoon\"\n\
\"pencil\"	7	\"c\"	\"calac\"	\"calgary\"\n\
\"pencil\"	7	\"e\"	\"emerald\"	\"vancouver\"\n\
\"pencil\"	7	\"i\"	\"intercon\"	\"saskatoon\"\n" },

	{ "hist union trans", "(hist^(date,item,id)) UNION-MERGE^(date,item,id) (trans^(date,item,id)) ",
"date	item	id	cost\n\
960204	\"mouse\"	\"e\"	200\n\
970101	\"disk\"	\"a\"	100\n\
970101	\"disk\"	\"e\"	200\n\
970101	\"mouse\"	\"c\"	200\n\
970102	\"mouse\"	\"c\"	200\n\
970103	\"pencil\"	\"e\"	300\n\
970201	\"eraser\"	\"c\"	150\n" },

	{ "trans union hist", "(trans^(date,item,id)) UNION-MERGE^(date,item,id) (hist^(date,item,id)) ",
"item	id	cost	date\n\
\"mouse\"	\"e\"	200	960204\n\
\"disk\"	\"a\"	100	970101\n\
\"disk\"	\"e\"	200	970101\n\
\"mouse\"	\"c\"	200	970101\n\
\"mouse\"	\"c\"	200	970102\n\
\"pencil\"	\"e\"	300	970103\n\
\"eraser\"	\"c\"	150	970201\n" },

	{ "hist minus trans", "(hist^(date,item,id)) MINUS^(date,item,id) (trans^(date,item,id)) ",
"date	item	id	cost\n\
970101	\"disk\"	\"e\"	200\n\
970102	\"mouse\"	\"c\"	200\n\
970103	\"pencil\"	\"e\"	300\n" },

	{ "trans minus hist", "(trans^(date,item,id)) MINUS^(date,item,id) (hist^(date,item,id)) ",
"item	id	cost	date\n\
\"mouse\"	\"e\"	200	960204\n\
\"mouse\"	\"c\"	200	970101\n\
\"eraser\"	\"c\"	150	970201\n" },

	{ "hist intersect trans", "(hist^(date,item,id)) INTERSECT^(date,item,id) (trans^(date,item,id)) ",
"date	item	id	cost\n\
970101	\"disk\"	\"a\"	100\n" },

	{ "trans intersect hist", "(trans^(date,item,id)) INTERSECT^(date,item,id) (hist^(date,item,id)) ",
"item	id	cost	date\n\
\"disk\"	\"a\"	100	970101\n" },
// 29
	{ "(trans union trans) intersect (hist union hist)", "((trans^(date,item,id)) UNION-MERGE^(date,item,id) (trans^(date,item,id)) ) INTERSECT^(date,item,id,cost) ((hist^(date,item,id)) UNION-MERGE^(date,item,id) (hist^(date,item,id))  TEMPINDEXN(date,item,id,cost) unique) ",
"item	id	cost	date\n\
\"disk\"	\"a\"	100	970101\n" },

	{ "(trans minus hist) union (trans intersect hist)", "((trans^(date,item,id)) MINUS^(date,item,id) (hist^(date,item,id)) ) UNION-MERGE^(date,item,id) ((trans^(date,item,id)) INTERSECT^(date,item,id) (hist^(date,item,id)) ) ",
"item	id	cost	date\n\
\"mouse\"	\"e\"	200	960204\n\
\"disk\"	\"a\"	100	970101\n\
\"mouse\"	\"c\"	200	970101\n\
\"eraser\"	\"c\"	150	970201\n" },

	{ "hist join customer", "(hist^(date,item,id)) JOIN n:1 on (id) (customer^(id))",
"date	item	id	cost	name	city\n\
970101	\"disk\"	\"a\"	100	\"axon\"	\"saskatoon\"\n\
970101	\"disk\"	\"e\"	200	\"emerald\"	\"vancouver\"\n\
970102	\"mouse\"	\"c\"	200	\"calac\"	\"calgary\"\n\
970103	\"pencil\"	\"e\"	300	\"emerald\"	\"vancouver\"\n" },

	{ "customer join hist", "(hist^(date,item,id)) JOIN n:1 on (id) (customer^(id))",
"date	item	id	cost	name	city\n\
970101	\"disk\"	\"a\"	100	\"axon\"	\"saskatoon\"\n\
970101	\"disk\"	\"e\"	200	\"emerald\"	\"vancouver\"\n\
970102	\"mouse\"	\"c\"	200	\"calac\"	\"calgary\"\n\
970103	\"pencil\"	\"e\"	300	\"emerald\"	\"vancouver\"\n" },

	{ "trans join inven", "(inven^(item)) JOIN 1:n on (item) (trans^(item))",
"item	qty	id	cost	date\n\
\"disk\"	5	\"a\"	100	970101\n\
\"mouse\"	2	\"e\"	200	960204\n\
\"mouse\"	2	\"c\"	200	970101\n" },

	{ "(trans union trans) join (inven union inven)", "((trans^(date,item,id)) UNION-MERGE^(date,item,id) (trans^(date,item,id)) ) JOIN n:n on (item) ((inven^(item)) UNION-MERGE^(item) (inven^(item)) )",
"item	id	cost	date	qty\n\
\"mouse\"	\"e\"	200	960204	2\n\
\"disk\"	\"a\"	100	970101	5\n\
\"mouse\"	\"c\"	200	970101	2\n" },

	{ "customer join alias", "(alias^(id)) JOIN 1:1 on (id) (customer^(id))",
"id	name2	name	city\n\
\"a\"	\"abc\"	\"axon\"	\"saskatoon\"\n\
\"c\"	\"trical\"	\"calac\"	\"calgary\"\n" },
// 36
	{ "customer join supplier", "(supplier^(city)) JOIN n:n on (name,city) (customer^(id) TEMPINDEX1(name,city))",
"supplier	name	city	id\n" },
// 37
	{ "(customer times inven) join trans", "(trans^(date,item,id)) JOIN n:1 on (id,item) ((customer^(id)) TIMES (inven^(item)) TEMPINDEXN(id,item) unique)",
"item	id	cost	date	name	city	qty\n\
\"mouse\"	\"e\"	200	960204	\"emerald\"	\"vancouver\"	2\n\
\"disk\"	\"a\"	100	970101	\"axon\"	\"saskatoon\"	5\n\
\"mouse\"	\"c\"	200	970101	\"calac\"	\"calgary\"	2\n" },

	{ "trans join customer join inven", "((trans^(date,item,id)) JOIN n:1 on (id) (customer^(id))) JOIN n:1 on (item) (inven^(item))",
"item	id	cost	date	name	city	qty\n\
\"mouse\"	\"e\"	200	960204	\"emerald\"	\"vancouver\"	2\n\
\"disk\"	\"a\"	100	970101	\"axon\"	\"saskatoon\"	5\n\
\"mouse\"	\"c\"	200	970101	\"calac\"	\"calgary\"	2\n" },

	{ "(trans where cost=100) union (trans where cost=200)", "(trans^(item) WHERE^(item)) UNION-DISJOINT (cost) (trans^(item) WHERE^(item)) ",
"item	id	cost	date\n\
\"disk\"	\"a\"	100	970101\n\
\"mouse\"	\"e\"	200	960204\n\
\"mouse\"	\"c\"	200	970101\n" },

	{ "(trans join customer) union (hist join customer)", "((trans^(date,item,id)) JOIN n:1 on (id) (customer^(id))) UNION-MERGE^(date,item,id) ((hist^(date,item,id)) JOIN n:1 on (id) (customer^(id))) ",
"item	id	cost	date	name	city\n\
\"mouse\"	\"e\"	200	960204	\"emerald\"	\"vancouver\"\n\
\"disk\"	\"a\"	100	970101	\"axon\"	\"saskatoon\"\n\
\"disk\"	\"e\"	200	970101	\"emerald\"	\"vancouver\"\n\
\"mouse\"	\"c\"	200	970101	\"calac\"	\"calgary\"\n\
\"mouse\"	\"c\"	200	970102	\"calac\"	\"calgary\"\n\
\"pencil\"	\"e\"	300	970103	\"emerald\"	\"vancouver\"\n\
\"eraser\"	\"c\"	150	970201	\"calac\"	\"calgary\"\n" },

	{ "(trans join customer) intersect (hist join customer)", "((trans^(date,item,id)) JOIN n:1 on (id) (customer^(id))) INTERSECT^(date,item,id) ((hist^(date,item,id)) JOIN n:1 on (id) (customer^(id))) ",
"item	id	cost	date	name	city\n\
\"disk\"	\"a\"	100	970101	\"axon\"	\"saskatoon\"\n" },

	{ "inven where qty + 1 > 5", "inven^(item) WHERE^(item) ((qty + 1) > 5)",
"item	qty\n\
\"disk\"	5\n\
\"pencil\"	7\n" },
/*
	{ "trans where \"mousee\" = item $ id", "trans^(date,item,id) WHERE^(date,item,id) (\"mousee\" == (item $ id))",
"item	id	cost	date\n\
\"mouse\"	\"e\"	200	960204\n" },
*/
	{ "inven where qty + 1 in (3,8)", "inven^(item) WHERE^(item) ((qty + 1) in (3,8))",
"item	qty\n\
\"mouse\"	2\n\
\"pencil\"	7\n" },

	{ "inven where qty + 1 in ()", "inven^(item) WHERE^(item) ((qty + 1) in ())",
"item	qty\n" },

	{ "trans extend x = cost * 1.1", "trans^(item) EXTEND x = (cost * 1.1)",
"item	id	cost	date	x\n\
\"disk\"	\"a\"	100	970101	110\n\
\"eraser\"	\"c\"	150	970201	165\n\
\"mouse\"	\"e\"	200	960204	220\n\
\"mouse\"	\"c\"	200	970101	220\n" },

	{ "trans extend x = 1 extend y = 2", "trans^(item) EXTEND x = 1, y = 2",
"item	id	cost	date	x	y\n\
\"disk\"	\"a\"	100	970101	1	2\n\
\"eraser\"	\"c\"	150	970201	1	2\n\
\"mouse\"	\"e\"	200	960204	1	2\n\
\"mouse\"	\"c\"	200	970101	1	2\n" },

	{ "trans extend x = 1 extend y = 2 extend z = 3", "trans^(item) EXTEND x = 1, y = 2, z = 3",
"item	id	cost	date	x	y	z\n\
\"disk\"	\"a\"	100	970101	1	2	3\n\
\"eraser\"	\"c\"	150	970201	1	2	3\n\
\"mouse\"	\"e\"	200	960204	1	2	3\n\
\"mouse\"	\"c\"	200	970101	1	2	3\n" },

	{ "hist extend x = item $ id", "hist^(date) EXTEND x = (item $ id)",
"date	item	id	cost	x\n\
970101	\"disk\"	\"a\"	100	\"diska\"\n\
970101	\"disk\"	\"e\"	200	\"diske\"\n\
970102	\"mouse\"	\"c\"	200	\"mousec\"\n\
970103	\"pencil\"	\"e\"	300	\"pencile\"\n" },
// 49
	{ "inven extend x = -qty sort x", "inven^(item) EXTEND x = -qty TEMPINDEXN(x)",
"item	qty	x\n\
\"pencil\"	7	-7\n\
\"disk\"	5	-5\n\
\"mouse\"	2	-2\n" },

	{ "inven extend x = (qty = 2 ? 222 : qty)", "inven^(item) EXTEND x = ((qty == 2) ? 222 : qty)",
"item	qty	x\n\
\"disk\"	5	5\n\
\"mouse\"	2	222\n\
\"pencil\"	7	7\n" },

	{ "trans summarize item, total cost", "trans^(item) SUMMARIZE-SEQ ^(item) (item) total_cost = total cost",
"item	total_cost\n\
\"disk\"	100\n\
\"eraser\"	150\n\
\"mouse\"	400\n" },

	{ "trans summarize item, x = total cost", "trans^(item) SUMMARIZE-SEQ ^(item) (item) x = total cost",
"item	x\n\
\"disk\"	100\n\
\"eraser\"	150\n\
\"mouse\"	400\n" },

	{ "trans summarize total cost", "trans^(item) SUMMARIZE-COPY total_cost = total cost",
"total_cost\n\
650\n" },

	{ "inven rename qty to x where x > 4", "inven^(item) WHERE^(item) RENAME qty to x",
"item	x\n\
\"disk\"	5\n\
\"pencil\"	7\n" },

	{ "inven extend x = qty where x > 4", "inven^(item) WHERE^(item) EXTEND x = qty",
"item	qty	x\n\
\"disk\"	5	5\n\
\"pencil\"	7	7\n" },

	{ "(inven leftjoin trans) where date = 960204",
	"(inven^(item)) LEFTJOIN 1:n on (item) (trans^(item)) WHERE (date == 960204)",
"item	qty	id	cost	date\n\
\"mouse\"	2	\"e\"	200	960204\n" },

	{ "inven leftjoin trans", "(inven^(item)) LEFTJOIN 1:n on (item) (trans^(item))",
"item	qty	id	cost	date\n\
\"disk\"	5	\"a\"	100	970101\n\
\"mouse\"	2	\"e\"	200	960204\n\
\"mouse\"	2	\"c\"	200	970101\n\
\"pencil\"	7	\"\"	\"\"	\"\"\n" },

	{ "customer leftjoin hist2", "(customer^(id)) LEFTJOIN 1:n on (id) (hist2^(id))",
"id	name	city	date	item	cost\n\
\"a\"	\"axon\"	\"saskatoon\"	970101	\"disk\"	100\n\
\"c\"	\"calac\"	\"calgary\"	\"\"	\"\"	\"\"\n\
\"e\"	\"emerald\"	\"vancouver\"	970102	\"disk\"	200\n\
\"e\"	\"emerald\"	\"vancouver\"	970103	\"pencil\"	300\n\
\"i\"	\"intercon\"	\"saskatoon\"	\"\"	\"\"	\"\"\n" },
// 59
	{ "customer leftjoin hist2 sort date", "(customer^(id)) LEFTJOIN 1:n on (id) (hist2^(id)) TEMPINDEXN(date)",
"id	name	city	date	item	cost\n\
\"c\"	\"calac\"	\"calgary\"	\"\"	\"\"	\"\"\n\
\"i\"	\"intercon\"	\"saskatoon\"	\"\"	\"\"	\"\"\n\
\"a\"	\"axon\"	\"saskatoon\"	970101	\"disk\"	100\n\
\"e\"	\"emerald\"	\"vancouver\"	970102	\"disk\"	200\n\
\"e\"	\"emerald\"	\"vancouver\"	970103	\"pencil\"	300\n" },

	// test moving where's past rename's
	{ "trans where cost = 200 rename cost to x where id is 'c'",
		"trans^(date,item,id) WHERE^(date,item,id) RENAME cost to x",
"item	id	x	date\n\
\"mouse\"	\"c\"	200	970101\n" },

	// test moving where's past extend's
	{ "trans where cost = 200 extend x = 1 where id is 'c'",
		"trans^(date,item,id) WHERE^(date,item,id) EXTEND x = 1",
"item	id	cost	date	x\n\
\"mouse\"	\"c\"	200	970101	1\n" },

	// test sorting of IN
	{ "customer where id in ('i', 'c', 'e', 'a') sort id", "customer^(id) WHERE^(id)",
"id	name	city\n\
\"a\"	\"axon\"	\"saskatoon\"\n\
\"c\"	\"calac\"	\"calgary\"\n\
\"e\"	\"emerald\"	\"vancouver\"\n\
\"i\"	\"intercon\"	\"saskatoon\"\n"},

	// test IN on dates
	{"dates where date in (#20010101, #20010301)", "dates^(date) WHERE^(date)",
"date\n\
#20010101\n\
#20010301\n" }

};

// tests without transform
Querystruct querytests2[] =
	{
	// 0
	{ "(((task join co)) join (cus where abbrev = 'a'))",
		"((co^(tnum)) JOIN 1:1 on (tnum) (task^(tnum))) JOIN n:1 on (cnum) (cus^(cnum) WHERE^(cnum))",
"tnum	signed	cnum	abbrev	name\n\
100	990101	1	\"a\"	\"axon\"\n\
104	990103	1	\"a\"	\"axon\"\n" },

	// 1
	{ "((task join (co where signed = 990103)) join (cus where abbrev = 'a'))",
		"((co^(tnum) WHERE^(tnum)) JOIN 1:1 on (tnum) (task^(tnum))) JOIN n:1 on (cnum) (cus^(cnum) WHERE^(cnum))",
"tnum	signed	cnum	abbrev	name\n\
104	990103	1	\"a\"	\"axon\"\n" },

	// 2
	{ "((task join (co where signed >= 990101)) join (cus where abbrev = 'a'))",
		"((co^(tnum) WHERE^(tnum)) JOIN 1:1 on (tnum) (task^(tnum))) JOIN n:1 on (cnum) (cus^(cnum) WHERE^(cnum))",
"tnum	signed	cnum	abbrev	name\n\
100	990101	1	\"a\"	\"axon\"\n\
104	990103	1	\"a\"	\"axon\"\n" },

	// 3
	{ "((task join (co where signed = 990103)) join cus)",
		"((co^(tnum) WHERE^(tnum)) JOIN 1:1 on (tnum) (task^(tnum))) JOIN n:1 on (cnum) (cus^(cnum))",
"tnum	signed	cnum	abbrev	name\n\
104	990103	1	\"a\"	\"axon\"\n" },

	// 4
	{ "(((task join co) where signed = 990103) join cus)",
		"((co^(tnum)) JOIN 1:1 on (tnum) (task^(tnum)) WHERE (signed == 990103)) JOIN n:1 on (cnum) (cus^(cnum))",
"tnum	signed	cnum	abbrev	name\n\
104	990103	1	\"a\"	\"axon\"\n" },

	// 5
	{ "((cus where abbrev = 'a') join ((task join co) where signed = 990103))",
		"((co^(tnum)) JOIN 1:1 on (tnum) (task^(tnum)) WHERE (signed == 990103)) JOIN n:1 on (cnum) (cus^(cnum) WHERE^(cnum))",
"tnum	signed	cnum	abbrev	name\n\
104	990103	1	\"a\"	\"axon\"\n" },

	// 6
	{ "(((task join co) where signed = 990103) join (cus where abbrev = 'a'))",
		"((co^(tnum)) JOIN 1:1 on (tnum) (task^(tnum)) WHERE (signed == 990103)) JOIN n:1 on (cnum) (cus^(cnum) WHERE^(cnum))",
"tnum	signed	cnum	abbrev	name\n\
104	990103	1	\"a\"	\"axon\"\n" },

	// 7
	{ "(((task join co) where signed = 990103) join (cus where abbrev = 'a' project cnum,abbrev,name))",
		"((co^(tnum)) JOIN 1:1 on (tnum) (task^(tnum)) WHERE (signed == 990103)) JOIN n:1 on (cnum) (cus^(cnum) WHERE^(cnum) PROJECT-COPY (cnum,abbrev,name))",
"tnum	signed	cnum	abbrev	name\n\
104	990103	1	\"a\"	\"axon\"\n" },

	// 8
	{ "(((task join co) where signed = 990103 rename tnum to tnum_new) join (cus where abbrev = 'a' project cnum,abbrev,name))",
		"((co^(tnum)) JOIN 1:1 on (tnum) (task^(tnum)) WHERE (signed == 990103) RENAME tnum to tnum_new) JOIN n:1 on (cnum) (cus^(cnum) WHERE^(cnum) PROJECT-COPY (cnum,abbrev,name))",
"tnum_new	signed	cnum	abbrev	name\n\
104	990103	1	\"a\"	\"axon\"\n" },

	// 9
	{ "((cus where abbrev = 'a' project cnum,abbrev,name) join ((task join co) where signed = 990103 rename tnum to tnum_new))",
		"((co^(tnum)) JOIN 1:1 on (tnum) (task^(tnum)) WHERE (signed == 990103) RENAME tnum to tnum_new) JOIN n:1 on (cnum) (cus^(cnum) WHERE^(cnum) PROJECT-COPY (cnum,abbrev,name))",
"tnum_new	signed	cnum	abbrev	name\n\
104	990103	1	\"a\"	\"axon\"\n" }
	};

#include "tempdb.h"
#include "sudate.h"
#include <vector>

class test_query : public Tests
	{
	OstreamStr errs;
	void adm(char* s)
		{
		database_admin(s);
		}
	int tran;
	void req(char* s)
		{
		except_if(! database_request(tran, s), "FAILED: " << s);
		}

	TEST(0, query)
		{
		TempDB tempdb;

		tran = theDB()->transaction(READWRITE);
		// so trigger searches won't give error
		adm("create stdlib (group, name, text) key(name,group)");

		// create customer file
		adm("create customer (id, name, city) key(id)");
		req("insert{id: \"a\", name: \"axon\", city: \"saskatoon\"} into customer");
		req("insert{id: \"c\", name: \"calac\", city: \"calgary\"} into customer");
		req("insert{id: \"e\", name: \"emerald\", city: \"vancouver\"} into customer");
		req("insert{id: \"i\", name: \"intercon\", city: \"saskatoon\"} into customer");

		// create hist file
		adm("create hist (date, item, id, cost) index(date) key(date,item,id)");
		req("insert{date: 970101, item: \"disk\", id: \"a\", cost: 100} into hist");
		req("insert{date: 970101, item: \"disk\", id: \"e\", cost: 200} into hist");
		req("insert{date: 970102, item: \"mouse\", id: \"c\", cost: 200} into hist");
		req("insert{date: 970103, item: \"pencil\", id: \"e\", cost: 300} into hist");

		// create hist2 file - for leftjoin test
		adm("create hist2 (date, item, id, cost) key(date) index(id)");
		req("insert{date: 970101, item: \"disk\", id: \"a\", cost: 100} into hist2");
		req("insert{date: 970102, item: \"disk\", id: \"e\", cost: 200} into hist2");
		req("insert{date: 970103, item: \"pencil\", id: \"e\", cost: 300} into hist2");

		// create trans file
		adm("create trans (item, id, cost, date) index(item) key(date,item,id)");
		req("insert{item: \"mouse\", id: \"e\", cost: 200, date: 960204} into trans");
		req("insert{item: \"disk\", id: \"a\", cost: 100, date: 970101} into trans");
		req("insert{item: \"mouse\", id: \"c\", cost: 200, date: 970101} into trans");
		req("insert{item: \"eraser\", id: \"c\", cost: 150, date: 970201} into trans");

		// create supplier file
		adm("create supplier (supplier, name, city) index(city) key(supplier)");
		req("insert{supplier: \"mec\", name: \"mtnequipcoop\", city: \"calgary\"} into supplier");
		req("insert{supplier: \"hobo\", name: \"hoboshop\", city: \"saskatoon\"} into supplier");
		req("insert{supplier: \"ebs\", name: \"ebssail&sport\", city: \"saskatoon\"} into supplier");
		req("insert{supplier: \"taiga\", name: \"taigaworks\", city: \"vancouver\"} into supplier");

		// create inven file
		adm("create inven (item, qty) key(item)");
		req("insert{item: \"disk\", qty: 5} into inven");
		req("insert{item: \"mouse\", qty:2} into inven");
		req("insert{item: \"pencil\", qty: 7} into inven");

		// create alias file
		adm("create alias(id, name2) key(id)");
		req("insert{id: \"a\", name2: \"abc\"} into alias");
		req("insert{id: \"c\", name2: \"trical\"} into alias");

		// create cus, task, co tables
		adm("create cus(cnum, abbrev, name) key(cnum) key(abbrev)");
		req("insert { cnum: 1, abbrev: 'a', name: 'axon' } into cus");
		req("insert { cnum: 2, abbrev: 'b', name: 'bill' } into cus");
		req("insert { cnum: 3, abbrev: 'c', name: 'cron' } into cus");
		req("insert { cnum: 4, abbrev: 'd', name: 'dick' } into cus");
		adm("create task(tnum, cnum) key(tnum)");
		req("insert { tnum: 100, cnum: 1 } into task");
		req("insert { tnum: 101, cnum: 2 } into task");
		req("insert { tnum: 102, cnum: 3 } into task");
		req("insert { tnum: 103, cnum: 4 } into task");
		req("insert { tnum: 104, cnum: 1 } into task");
		req("insert { tnum: 105, cnum: 2 } into task");
		req("insert { tnum: 106, cnum: 3 } into task");
		req("insert { tnum: 107, cnum: 4 } into task");
		adm("create co(tnum, signed) key(tnum)");
		req("insert { tnum: 100, signed: 990101 } into co");
		req("insert { tnum: 102, signed: 990102 } into co");
		req("insert { tnum: 104, signed: 990103 } into co");
		req("insert { tnum: 106, signed: 990104 } into co");

		adm("create dates(date) key(date)");
		req("insert { date: #20010101 } into dates");
		req("insert { date: #20010102 } into dates");
		req("insert { date: #20010301 } into dates");
		req("insert { date: #20010401 } into dates");

		verify(theDB()->commit(tran));

		int i = 0;
		try
			{
			for (i = 0; i < sizeof querytests / sizeof (Querystruct); ++i)
				process(i);
			// test without transform
			for (i = 0; i < sizeof querytests2 / sizeof (Querystruct); ++i)
				process2(i);
			}
		catch (const Except& e)
			{
			errs << i << ": " << e << endl;
			}
		if (*errs.str())
			except(errs.str());
		}

	void process(int i)
		{
		char* s = querytests[i].query;
		Query* q = query(s);
		int tran = theDB()->transaction(READONLY);
		q->set_transaction(tran);

		OstreamStr exp;
		exp << *q;
		if (0 != strcmp(querytests[i].explain, exp.str()))
			errs << i << ": " << s <<
				"\n\tgot: '" << exp.str() << "'" <<
				"\n\tnot: '" << querytests[i].explain << "'" << endl;
		Header hdr = q->header();

		{ // read next
		OstreamStr out;
		for (Fields f = hdr.columns(); ! nil(f); ++f)
			out << *f << (nil(cdr(f)) ? "" : "\t");
		out << "\n";
		Row row;
		while (Query::Eof != (row = q->get(NEXT)))
			{
			for (Fields f = hdr.columns(); ! nil(f); ++f)
				out << row.getval(hdr, *f) << (nil(cdr(f)) ? "" : "\t");
			out << endl;
			}
		q->close(q);
		if (0 != strcmp(querytests[i].result, out.str()))
			errs << i << ": " << s <<
				"\n\tgot: '" << out.str() << "'" <<
				"\n\tnot: '" << querytests[i].result << "'" << endl;
		}

		{ // read prev
		Query* q = query(s);
		q->set_transaction(tran);
		OstreamStr out;
		for (Fields f = hdr.columns(); ! nil(f); ++f)
			out << *f << (nil(cdr(f)) ? "" : "\t");
		out << "\n";
		Row row;
		std::vector<Row> rows;
		while (Query::Eof != (row = q->get(PREV)))
			rows.push_back(row);
		for (int j = rows.size() - 1; j >= 0; --j)
			{
			for (Fields f = hdr.columns(); ! nil(f); ++f)
				out << rows[j].getval(hdr, *f) << (nil(cdr(f)) ? "" : "\t");
			out << endl;
			}
		q->close(q);
		if (0 != strcmp(querytests[i].result, out.str()))
			errs << "prev " << i << ": " << s <<
				"\n\tgot: '" << out.str() << "'" <<
				"\n\tnot: '" << querytests[i].result << "'" << endl;
		}

		verify(theDB()->commit(tran));
		extern int tempdest_inuse;
		verify(tempdest_inuse == 0);
		}

	// query(s) without transform
	void process2(int i)
		{
		char* s = querytests2[i].query;
		int tran = theDB()->transaction(READONLY);

		{ // read next
		Query* q = parse_query(s);
		Fields order;
		if (QSort* s = dynamic_cast<QSort*>(q))
			{
			order = s->segs;
			q = s->source;
			}
		// NO TRANSFORM // q = q->transform();
		if (q->optimize(order, q->columns(), Fields(), false, true) >= IMPOSSIBLE)
			except("impossible");
		q = q->addindex();

		q->set_transaction(tran);

		OstreamStr exp;
		exp << *q;
		if (0 != strcmp(querytests2[i].explain, exp.str()))
			errs << i << ": " << s <<
				"\n\tgot: '" << exp.str() << "'" <<
				"\n\tnot: '" << querytests2[i].explain << "'" << endl;
		Header hdr = q->header();

		OstreamStr out;
		for (Fields f = hdr.columns(); ! nil(f); ++f)
			out << *f << (nil(cdr(f)) ? "" : "\t");
		out << "\n";
		Row row;
		while (Query::Eof != (row = q->get(NEXT)))
			{
			for (Fields f = hdr.columns(); ! nil(f); ++f)
				out << row.getval(hdr, *f) << (nil(cdr(f)) ? "" : "\t");
			out << endl;
			}
		q->close(q);
		if (0 != strcmp(querytests2[i].result, out.str()))
			errs << i << ": next: " << s <<
				"\n\tgot: '" << out.str() << "'" <<
				"\n\tnot: '" << querytests2[i].result << "'" << endl;
		}

		{ // read prev
		Query* q = parse_query(s);
		Fields order;
		if (QSort* s = dynamic_cast<QSort*>(q))
			{
			order = s->segs;
			q = s->source;
			}
		// NO TRANSFORM // q = q->transform();
		if (q->optimize(order, q->columns(), Fields(), false, true) >= IMPOSSIBLE)
			except("impossible");
		q = q->addindex();

		q->set_transaction(tran);

		Header hdr = q->header();

		OstreamStr out;
		for (Fields f = hdr.columns(); ! nil(f); ++f)
			out << *f << (nil(cdr(f)) ? "" : "\t");
		out << "\n";
		Row row;
		std::vector<Row> rows;
		while (Query::Eof != (row = q->get(PREV)))
			rows.push_back(row);
		q->close(q);
		for (int j = rows.size() - 1; j >= 0; --j)
			{
			for (Fields f = hdr.columns(); ! nil(f); ++f)
				out << rows[j].getval(hdr, *f) << (nil(cdr(f)) ? "" : "\t");
			out << endl;
			}
		if (0 != strcmp(querytests2[i].result, out.str()))
			errs << i << ": prev: " << s <<
				"\n\tgot: '" << out.str() << "'" <<
				"\n\tnot: '" << querytests2[i].result << "'" << endl;
		}

		verify(theDB()->commit(tran));
		extern int tempdest_inuse;
		verify(tempdest_inuse == 0);
		}
	TEST(1, order)
		{
		TempDB tempdb;
		Query* q;

		q = query("tables rename tablename to tname sort totalsize");
		assert_eq(q->ordering(), lisp(gcstring("totalsize")));

		q = query("tables rename tablename to tname sort table");
		assert_eq(q->ordering(), lisp(gcstring("table")));

		q = query("columns project table, field sort table");
		assert_eq(q->ordering(), lisp(gcstring("table")));
		}
#define TESTUP(q) except_if(! query(q)->updateable(), #q "should be updateable")
#define TESTNUP(q) except_if(query(q)->updateable(), #q "should NOT be updateable")
	TEST(2, updateable)
		{
		TempDB tempdb;
		TESTUP("tables");
		TESTNUP("history(tables)");
		TESTUP("tables extend xyz = 123");
		TESTUP("tables project table");
		TESTNUP("columns project table");
		TESTUP("tables sort totalsize");
		TESTUP("tables sort reverse totalsize");
		TESTUP("tables rename totalsize to bytes");
		TESTUP("tables where totalsize > 1000");
		TESTNUP("tables summarize count");
		TESTNUP("tables join columns");
		TESTNUP("tables union columns");
		TESTNUP("tables union columns extend xyz = 123");
		};
	TEST(3, history)
		{
		TempDB tempdb;
		adm("create cus(cnum, abbrev, name) key(cnum) key(abbrev)");
		
		tran = theDB()->transaction(READONLY);
		Query* q = query("history(cus)");
		q->set_transaction(tran);
		assert_eq(q->get(NEXT), Query::Eof);
		q->close(q);
		verify(theDB()->commit(tran));		
		
		tran = theDB()->transaction(READWRITE);
		req("insert { cnum: 1, abbrev: 'a', name: 'axon' } into cus");
		verify(theDB()->commit(tran));		
		
		tran = theDB()->transaction(READONLY);
		q = query("history(cus)");
		q->set_transaction(tran);
		Row row = q->get(NEXT);
		verify(row != Query::Eof);
		Header hdr = q->header();
		q->close(q);
		verify(theDB()->commit(tran));		
		OstreamStr os;
		os << "cnum: " << row.getval(hdr, "cnum") <<
			" abbrev: " << row.getval(hdr, "abbrev") <<
			" name: " << row.getval(hdr, "name") <<
			" _action: " << row.getval(hdr, "_action");
		assert_eq(os.str(), gcstring("cnum: 1 abbrev: \"a\" name: \"axon\" _action: \"create\""));
		SuDate now;
		SuDate* date = force<SuDate*>(row.getval(hdr, "_date"));
		int diff = SuDate::minus_ms(&now, date);
		if (diff < 0 || 1000 < diff)
			except_err("diff " << diff);
		}
	};
REGISTER(test_query);

class test_prefixed : public Tests
	{
	TEST(0, main)
		{
		Fields index_nil;
		Fields by_nil;
		Lisp<Fixed> fixed_nil;
		verify(prefixed(index_nil, by_nil, fixed_nil));
		Fields index_a = lisp(gcstring("a"));
		verify(prefixed(index_a, by_nil, fixed_nil));
		Fields by_a = lisp(gcstring("a"));
		verify(! prefixed(index_nil, by_a, fixed_nil));
		verify(prefixed(index_a, by_a, fixed_nil));
		Fields index_ab = lisp(gcstring("a"), gcstring("b"));
		verify(prefixed(index_ab, by_nil, fixed_nil));
		verify(prefixed(index_ab, by_a, fixed_nil));

		Fields by_b = lisp(gcstring("b"));
		verify(! prefixed(index_ab, by_b, fixed_nil));
		Lisp<Fixed> fixed_a = lisp(Fixed("a", 1));
		verify(prefixed(index_ab, by_b, fixed_a));
		Lisp<Fixed> fixed_a2 = lisp(Fixed("a", lisp(Value(1), Value(2))));
		verify(! prefixed(index_ab, by_b, fixed_a2));
		Fields index_abc = lisp(gcstring("a"), gcstring("b"), gcstring("c"));
		Fields by_ac = lisp(gcstring("a"), gcstring("c"));
		Lisp<Fixed> fixed_b = lisp(Fixed("b", 1));
		verify(prefixed(index_abc, by_ac, fixed_b));
		verify(prefixed(index_ab, by_a, fixed_a));
		verify(prefixed(index_ab, by_a, fixed_b));
		Fields by_abc = lisp(gcstring("a"), gcstring("b"), gcstring("c"));
		Lisp<Fixed> fixed_c = lisp(Fixed("c", 1));
		verify(prefixed(index_ab, by_abc, fixed_c));

		Fields index_abce = lisp(gcstring("a"), gcstring("b"), gcstring("c"), gcstring("e"));
		Fields by_acde = lisp(gcstring("a"), gcstring("c"), gcstring("d"), gcstring("e"));
		Lisp<Fixed> fixed_bd = lisp(Fixed("b", 1), Fixed("d", 2));
		verify(prefixed(index_abce, by_acde, fixed_bd));
		}
	};
REGISTER(test_prefixed);
