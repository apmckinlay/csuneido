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

#include "qsummarize.h"
#include "queryimp.h"
#include "sunumber.h"
#include "sustring.h"
#include "suobject.h" // for List
#include <map>
using namespace std;

class Strategy
	{
public:
	Strategy(Summarize* s) : q(s), source(s->source)
		{ }
	virtual Row get(Dir dir, bool rewound) = 0;
	virtual void select(const Fields& index,
			const Record& from, const Record& to) = 0;
	virtual ~Strategy()
		{ }
	friend class Summarize;
protected:
	Lisp<class Summary*> funcSums();
	void initSums(Lisp<class Summary*> sums);
	Row makeRow(Record byRec, Lisp<class Summary*> sums);

	Summarize* q;
	Query* source;
	Dir curdir;
	Keyrange sel;
	};

class SeqStrategy : public Strategy
	{
public:
	SeqStrategy(Summarize* source);
	Row get(Dir dir, bool rewound);
	void select(const Fields& index, const Record& from, const Record& to);
private:
	bool equal();

	Lisp<class Summary*> sums;
	Row nextrow;
	Row currow;
	};

class MapStrategy : public Strategy
	{
public:
	MapStrategy(Summarize* source);
	Row get(Dir dir, bool rewound);
	void select(const Fields& index, const Record& from, const Record& to);
private:
	void process();

	typedef map<Record, Lisp<class Summary*> > Map;
	Map results;
	Map::iterator begin, end, iter;
	bool first;
	};

const Fields none;

Query* Query::make_summarize(Query* source, const Fields& p, const Fields& c,
	const Fields& f, const Fields& o)
	{
	return new Summarize(source, p, c, f, o);
	}

Summarize::Summarize(Query* source, const Fields& p, const Fields& c,
	const Fields& f, const Fields& o)
	: Query1(source), by(p), cols(c), funcs(f), on(o), strategy(NONE),
		first(true), rewound(true)
	{
	if (! subset(source->columns(), by))
		except("summarize: nonexistent columns: " <<
			difference(by, source->columns()));

	if (nil(by) || by_contains_key())
		strategy = COPY;
	}

bool Summarize::by_contains_key()
	{
	// check if project contain candidate key
	Indexes k = source->keys();
	for (; ! nil(k); ++k)
		if (subset(by, *k))
			break ;
	return ! nil(k);
	}

void Summarize::out(Ostream& os) const
	{
	const char* s[] = { "", "-COPY", "-SEQ", "-MAP" };
	os << *source << " SUMMARIZE" << s[strategy];
	if (! nil(via))
		os << " ^" << via;
	if (! nil(by))
		os << " " << by;
	os << " ";
	for (Fields c = cols, f = funcs, o = on; ! nil(c); ++c, ++f, ++o)
		{
		os << *c << " = " << *f << " " << *o;
		if (! nil(cdr(c)))
			os << ", ";
		}
	}

Indexes Summarize::indexes()
	{
	if (strategy == COPY)
		return source->indexes();
	else
		{
		Indexes idxs;
		for (Indexes src = source->indexes(); ! nil(src); ++src)
			if (prefix_set(*src, by))
				idxs.push(*src);
		return idxs;
		}
	}

Indexes Summarize::keys()
	{
	Indexes keys;
	for (Indexes k = source->keys(); ! nil(k); ++k)
		if (subset(by, *k))
			keys.push(*k);
	return nil(keys) ? Indexes(by) : keys;
	}

double Summarize::optimize2(const Fields& index, const Fields& needs,
	const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	const Fields srcneeds = set_union(erase(on, gcstring("")),
		difference(needs, cols));

	if (strategy == COPY)
		return source->optimize(index, srcneeds, by, is_cursor, freeze);

	Indexes indexes;
	if (nil(index))
		indexes = source->indexes();
	else
		{
		Lisp<Fixed> fixed = source->fixed();
		for (Indexes idxs = source->indexes(); ! nil(idxs); ++idxs)
			if (prefixed(*idxs, index, fixed))
				indexes.push(*idxs);
		}
	Fields best_index;
	double best_cost = IMPOSSIBLE;
	best_prefixed(indexes, by, srcneeds, is_cursor, best_index, best_cost);
	if (nil(best_index) && prefix(by, index))
		{
		// accumulate results in memory
		// doesn't require any order, can only supply in order of "by"
		strategy = MAP;
		return source->optimize(none, srcneeds, by, is_cursor, freeze);
		}
	if (nil(best_index))
		return IMPOSSIBLE;
	strategy = SEQUENTIAL;
	if (! freeze)
		return best_cost;
	via = best_index;
	return source->optimize(best_index, srcneeds, Fields(), is_cursor, freeze);
	}

// functions ========================================================

class Summary
	{
public:
	Summary()
		{ init(); }
	virtual void init()
		{ }
	virtual void add(Value x) = 0;
	virtual Value result() = 0;
	};

class Total : public Summary
	{
public:
	void init()
		{ total = 0; }
	void add(Value x)
		{
		try
			{ total = total + x; }
		catch (...)
			{ }
		}
	Value result()
		{ return total; }
private:
	Value total;
	};

class Average : public Summary
	{
public:
	void init()
		{ count = 0; total = 0; }
	void add(Value x)
		{
		try
			{ total = total + x; ++count; }
		catch (...)
			{ }
		}
	Value result()
		{ return count ? Value(total / count) : Value(SuString::empty_string); }
private:
	int count;
	Value total;
	};

class Count : public Summary
	{
public:
	void init()
		{ count = 0; }
	void add(Value)
		{ ++count; }
	Value result()
		{ return count; }
private:
	int count;
	};

class Max : public Summary
	{
public:
	void init()
		{ max = Value(); }
	void add(Value x)
		{ if (! max || x > max) max = x; }
	Value result()
		{ return max; }
private:
	Value max;
	};

class Min : public Summary
	{
public:
	void init()
		{ min = Value(); }
	void add(Value x)
		{ if (! min || x < min) min = x; }
	Value result()
		{ return min; }
private:
	Value min;
	};

class List : public Summary
	{
public:
	void init()
		{ list = new SuObject; }
	void add(Value x)
		{
		if (list->find(x) == SuFalse)
			list->add(x);
		}
	Value result()
		{ return list; }
private:
	SuObject* list;
	};

Summary* summary(const gcstring& type)
	{
	if (type == "total")
		return new Total;
	else if (type == "average")
		return new Average;
	else if (type == "count")
		return new Count;
	else if (type == "max")
		return new Max;
	else if (type == "min")
		return new Min;
	else if (type == "list")
		return new List;
	error("unknown summary type");
	return 0;
	}

//===================================================================

Header Summarize::header()
	{
	if (first)
		iterate_setup();
	Fields flds = concat(by, cols);
	return Header(lisp(Fields(), flds), flds);
	}

Row Summarize::get(Dir dir)
	{
	if (first)
		iterate_setup();
	bool wasRewound = rewound;
	rewound = false;
	return strategyImp->get(dir, wasRewound);
	}

void Summarize::iterate_setup()
	{
	first = false;
	hdr = source->header();
	if (strategy == MAP)
		strategyImp = new MapStrategy(this);
	else
		strategyImp = new SeqStrategy(this);
	}

void Summarize::select(const Fields& index, const Record& from, const Record& to)
	{
	strategyImp->select(index, from, to);
	strategyImp->sel.org = from;
	strategyImp->sel.end = to;
	rewound = true;
	}

void Summarize::rewind()
	{
	source->rewind();
	rewound = true;
	}

//===================================================================

Lisp<class Summary*> Strategy::funcSums()
	{
	Lisp<class Summary*> sums;
	for (Fields f = q->funcs; ! nil(f); ++f)
		sums.push(summary(*f));
	return sums.reverse();
	}

void Strategy::initSums(Lisp<class Summary*> sums)
	{
	for (Lisp<Summary*> s = sums; ! nil(s); ++s)
		(*s)->init();
	}

Row Strategy::makeRow(Record r, Lisp<class Summary*> sums)
	{
	for (Lisp<class Summary*> s = sums; ! nil(s); ++s)
		r.addval((*s)->result());
	static Record emptyrec;
	return Row(lisp(emptyrec, r));
	}

#define Eof Query::Eof

SeqStrategy::SeqStrategy(Summarize* summarize) : Strategy(summarize)
 	{
	sums = funcSums();
	}

Row SeqStrategy::get(Dir dir, bool rewound)
	{
	if (rewound)
		{
		curdir = dir;
		currow = Row();
		nextrow = source->get(dir);
		if (nextrow == Eof)
			return Eof;
		}

	// if direction changes, have to skip over previous result
	if (dir != curdir)
		{
		if (nextrow == Eof)
			source->rewind();
		do
			nextrow = source->get(dir);
			while (nextrow != Eof && equal());
		curdir = dir;
		}

	if (nextrow == Eof)
		return Eof;

	currow = nextrow;
	initSums(sums);
	do
		{
		if (nextrow == Eof)
			break ;
		Lisp<Summary*> s = sums;
		for (Fields o = q->on; ! nil(o); ++o, ++s)
			(*s)->add(nextrow.getval(q->hdr, *o));
		nextrow = source->get(dir);
		}
		while (equal());
	// output after reading a group

	Record byRec = row_to_key(q->hdr, currow, q->by);
	return makeRow(byRec, sums);
	}

bool SeqStrategy::equal()
	{
	if (nextrow == Eof)
		return false;
	for (Fields f = q->by; ! nil(f); ++f)
		if (currow.getval(q->hdr, *f) != nextrow.getval(q->hdr, *f))
			return false;
	return true;
	}

void SeqStrategy::select(const Fields& index, const Record& from, const Record& to)
	{
	source->select(index, from, to);
	}

//===================================================================

MapStrategy::MapStrategy(Summarize* summarize) : Strategy(summarize), first(true)
	{
	}

Row MapStrategy::get(Dir dir, bool rewound)
	{
	if (first)
		{
		process();
		first = false;
		}
	if (rewound)
		{
		begin = results.lower_bound(sel.org);
		end = results.upper_bound(sel.end);
		iter = dir == NEXT ? begin : end;
		curdir = dir;
		}

	if (dir != curdir)
		{
		if (dir == PREV)
			--iter;
		else
			++iter;
		curdir = dir;
		}

	if (iter == (dir == NEXT ? end : begin))
		return Eof;
	if (dir == PREV)
		--iter;
	Row row = makeRow(iter->first.dup(), iter->second);
	if (dir == NEXT)
		++iter;
	return row;
	}

void MapStrategy::process()
	{
	results.clear();
	Row row;
	while (Eof != (row = source->get(NEXT)))
		{
		Record byRec = row_to_key(q->hdr, row, q->by);
		Map::iterator iter = results.find(byRec);
		Lisp<Summary*> sums;
		if (iter == results.end())
			{
			sums = funcSums();
			initSums(sums);
			results[byRec] = sums;
			}
		else
			sums = iter->second;

		Lisp<Summary*> s = sums;
		for (Fields o = q->on; ! nil(o); ++o, ++s)
			(*s)->add(row.getval(q->hdr, *o));
		}
	}

void MapStrategy::select(const Fields& index, const Record& from, const Record& to)
	{
	verify(prefix(q->by, index));
	sel.org = from;
	sel.end = to;

	}
