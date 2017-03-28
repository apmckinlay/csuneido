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

#define Eof Query::Eof

class Strategy
	{
public:
	explicit Strategy(Summarize* s) : q(s), source(s->source)
		{ }
	virtual ~Strategy() = default;
	virtual Row get(Dir dir, bool rewound) = 0;
	virtual void select(const Fields& index,
			const Record& from, const Record& to) = 0;
	friend class Summarize;
protected:
	Lisp<class Summary*> funcSums();
	void initSums(Lisp<class Summary*> sums);
	Row makeRow(Record byRec, Lisp<class Summary*> sums);

	Summarize* q;
	Query* source;
	Dir curdir = NEXT;
	Keyrange sel;
	};

// used when we can read the data in order of "by" (or when "by" is empty)
class SeqStrategy : public Strategy
	{
public:
	explicit SeqStrategy(Summarize* q);
	Row get(Dir dir, bool rewound) override;
	void select(const Fields& index, const Record& from, const Record& to) override;
private:
	bool equal();

	Lisp<class Summary*> sums;
	Row nextrow;
	Row currow;
	};

// used when data is read unordered
class MapStrategy : public Strategy
	{
public:
	explicit MapStrategy(Summarize* q);
	Row get(Dir dir, bool rewound) override;
	void select(const Fields& index, const Record& from, const Record& to) override;
private:
	void process();

	typedef map<Record, Lisp<class Summary*> > Map;
	Map results;
	Map::iterator begin, end, iter;
	bool first;
	};

// used for "summarize min/max index-field"
// where we can just read first/last using index
class IdxStrategy : public Strategy
{
public:
	explicit IdxStrategy(Summarize* q) : Strategy(q)
		{ }
	Row get(Dir dir, bool rewound) override;
	void select(const Fields& index, const Record& from, const Record& to) override;
private:
	Fields selIndex;
};

//===================================================================

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
	wholeRecord = minmax1() && source->keys().member(on);
	}

void Summarize::out(Ostream& os) const
	{
	const char* s[] = { "", "-SEQ", "-MAP", "-IDX" };
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

double Summarize::nrecords()
	{
	double nr = source->nrecords();
	return nr < 1 ? nr
		: nil(by) ? 1
		: by_contains_key() ? nr
		: nr / 2;					//TODO review this estimate
}

Indexes Summarize::indexes()
	{
	if (nil(by) || by_contains_key())
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

//===================================================================

double Summarize::optimize2(const Fields& index, const Fields& needs,
	const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	const Fields srcneeds =
		set_union(erase(on, gcstring("")), difference(needs, cols));

	double seq_cost = seqCost(index, srcneeds, is_cursor, false);
	double idx_cost = idxCost(is_cursor, false);
	double map_cost = mapCost(index, srcneeds, is_cursor, false);

	if (!freeze)
		return min(seq_cost, min(idx_cost, map_cost));

	if (seq_cost <= idx_cost && seq_cost <= map_cost)
		return seqCost(index, srcneeds, is_cursor, true);
	else if (idx_cost <= map_cost)
		return idxCost(is_cursor, true);
	else
		return mapCost(index, srcneeds, is_cursor, true);
	}

double Summarize::seqCost(const Fields& index, const Fields& srcneeds,
	bool is_cursor, bool freeze) {
	if (freeze)
		strategy = SEQ;
	if (nil(by) || by_contains_key())
		return source->optimize(nil(by) ? none : index,
			srcneeds, by, is_cursor, freeze);
	else
		{
		Fields best_index;
		double best_cost = IMPOSSIBLE;
		best_prefixed(sourceIndexes(index), by, srcneeds, is_cursor, 
			best_index, best_cost);
		if (!freeze || best_cost >= IMPOSSIBLE)
			return best_cost;
		via = best_index;
		return source->optimize1(best_index, srcneeds, none, is_cursor, freeze);
		}
	}
Indexes Summarize::sourceIndexes(const Fields& index) const
	{
	if (nil(index))
		return source->indexes();
	else
		{
		Indexes indexes;
		Lisp<Fixed> fixed = source->fixed();
		for (Indexes idxs = source->indexes(); !nil(idxs); ++idxs)
			if (prefixed(*idxs, index, fixed))
				indexes.push(*idxs);
		return indexes.reverse();
		}
	}

double Summarize::idxCost(bool is_cursor, bool freeze)
	{ 
	if (!minmax1())
		return IMPOSSIBLE;
	// using optimize1 to bypass tempindex
	// dividing by nrecords since we're only reading one record
	auto nr = max(1.0, source->nrecords());
	double cost = source->optimize1(on, none, none, is_cursor, freeze) / nr;
	if (freeze)
		{
		strategy = IDX;
		via = on;
		}
	return cost;
	}

bool Summarize::minmax1() const 
	{
	if (!nil(by) || funcs.size() != 1)
		return false;
	gcstring fn = funcs[0];
	return fn == "min" || fn == "max";
	}

bool Summarize::by_contains_key() const
	{
	// check if project contain candidate key
	Indexes k = source->keys();
	for (; !nil(k); ++k)
		if (subset(by, *k))
			break;
	return !nil(k);
	}

double Summarize::mapCost(const Fields& index, const Fields& srcneeds,
	bool is_cursor, bool freeze)
	{
	// can only provide 'by' as index
	if (!prefix(by, index))
		return IMPOSSIBLE;
	// using optimize1 to bypass tempindex
	// add 50% for map overhead
	double cost = 1.5 *
		source->optimize1(none, srcneeds, none, is_cursor, freeze);
	if (freeze)
		strategy = MAP;
	return cost;
	}

// functions ========================================================

class Summary
	{
public:
	Summary()
		{ }
	virtual ~Summary() = default;
	virtual void init()
		{ }
	virtual void add(Value x)
		{ add(Eof, x); }
	virtual void add(Row row, Value x)
		{ add(x); }
	virtual Value result() = 0;
	virtual Row getRow()
		{ return Eof; }
	};

class Total : public Summary
	{
public:
	void init() override
		{ total = 0; }
	void add(Value x) override
		{
		try
			{ total = total + x; }
		catch (...)
			{ }
		}
	Value result() override
		{ return total; }
private:
	Value total;
	};

class Average : public Summary
	{
public:
	void init() override
		{ count = 0; total = 0; }
	void add(Value x) override
		{
		try
			{ total = total + x; ++count; }
		catch (...)
			{ }
		}
	Value result() override
		{ return count ? Value(total / count) : Value(SuString::empty_string); }
private:
	int count = 0;
	Value total;
	};

class Count : public Summary
	{
public:
	void init() override
		{ count = 0; }
	void add(Value) override
		{ ++count; }
	Value result() override
		{ return count; }
private:
	int count = 0;
	};

class MinMax : public Summary
	{
	void init() override
		{ val = Value(); }
	Value result() override
		{ return val; }
	Row getRow() override
		{ return row; }
protected:
	Row row;
	Value val;
	};

class Max : public MinMax
	{
public:
	void add(Row r, Value x) override
		{
		if (! val || x > val)
			{
			row = r;
			val = x;
			}
		}
	};

class Min : public MinMax
	{
public:
	void add(Row r, Value x) override
		{
		if (! val || x < val)
			{
			row = r;
			val = x;
			}
		}
	};

class List : public Summary
	{
public:
	void init() override
		{ list = new SuObject; }
	void add(Value x) override
		{
		if (list->find(x) == SuFalse)
			list->add(x);
		}
	Value result() override
		{ return list; }
private:
	SuObject* list = nullptr;
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
	}

//===================================================================

Fields Summarize::columns()
	{
	return wholeRecord
		? set_union(cols, source->columns())
		: set_union(by, cols);
	}

Header Summarize::header()
	{
	if (first)
		iterate_setup();
	if (wholeRecord)
		return source->header() + Header(lisp(none, cols), cols);
	else
		{
		Fields flds = concat(by, cols);
		return Header(lisp(none, flds), flds);
		}
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
	if (strategy == IDX)
		strategyImp = new IdxStrategy(this);
	else if (strategy == MAP)
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
	return Row(lisp(Record::empty, r));
	}

//===================================================================

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
			(*s)->add(nextrow, nextrow.getval(q->hdr, *o));
		nextrow = source->get(dir);
		}
		while (equal());
	// output after reading a group

	Record byRec = row_to_key(q->hdr, currow, q->by);
	Row row = makeRow(byRec, sums);
	if (q->wholeRecord)
		row = Row(sums[0]->getRow()) + row;
	return row;
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
	// because of fixed, this index may not be the same as the source index (via)
	if (prefix(q->via, index) || (from == keymin && to == keymax))
		source->select(q->via, from, to);
	else
		except_err("Summarize SeqStrategy by " << q->via <<
			" doesn't handle select(" << index << " from " << from << " to " << to << ")");
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
		Map::iterator itr = results.find(byRec);
		Lisp<Summary*> sums;
		if (itr == results.end())
			{
			sums = funcSums();
			initSums(sums);
			results[byRec] = sums;
			}
		else
			sums = itr->second;

		Lisp<Summary*> s = sums;
		for (Fields o = q->on; ! nil(o); ++o, ++s)
			(*s)->add(row.getval(q->hdr, *o));
		}
	}

void MapStrategy::select(const Fields& index, const Record& from, const Record& to)
	{
	verify(prefix(q->by, index));
	}

//===================================================================

Row IdxStrategy::get(Dir, bool rewound)
	{
	if (!rewound)
		return Eof;
	Dir dir = q->funcs[0] == "min" ? NEXT : PREV;
	Row row = source->get(dir);
	if (row == Eof)
		return Eof;
	if (! nil(selIndex)) {
		Record key = row_to_key(q->hdr, row, selIndex);
		if (key < sel.org || sel.end > key)
			return Eof;
	}
	Record r;
	r.addraw(row.getraw(q->hdr, q->on[0]));
	Row result = Row(Record::empty) + Row(r);
	if (q->wholeRecord)
		result = row + result;
	return result;
	}

void IdxStrategy::select(const Fields& index, const Record& from, const Record& to)
	{
    selIndex = index;
	}
