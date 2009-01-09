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

#define LOGGING

#ifdef LOGGING
#include "ostreamfile.h"
#include "trace.h"
#define LOG(stuff) TRACE(SELECT, stuff )
//OstreamFile logfile("select.txt", "w");
//#define LOG(stuff) logfile << stuff << endl; logfile.flush()
#else
#define LOG(stuff)
#endif

#include "qselect.h"
#include "queryimp.h"
#include "qtable.h"
#include "hashmap.h"
#include "array.h"
#include "database.h"
#include "qexprimp.h"
#include "qscanner.h"
#include "thedb.h"
#include "suboolean.h"
// these are needed by transform
#include "qrename.h"
#include "qextend.h"
#include "qsummarize.h"
#include "qproject.h"
#include "qunion.h"
#include "qintersect.h"
#include "qdifference.h"
#include "qproduct.h"
#include "qjoin.h"
#include "pack.h"
#include "sustring.h"
#include "trace.h" // for slowqueries
#include "ostreamfile.h" // for slowqueries
#include <math.h> // for fabs

struct FilterTreeSlot
	{
	typedef long Key;
	Key key;
	long adr;
	FilterTreeSlot()
		{ }
	explicit FilterTreeSlot(Key k, long a = 0) : key(k), adr(a)
		{ }
	void copy(const FilterTreeSlot& from)
		{
		key = from.key;
		adr = from.adr;
		}
	bool operator<(const FilterTreeSlot& y) const
		{ return key < y.key; }
	bool operator==(const FilterTreeSlot& y) const
		{ return key == y.key; }
	};
typedef Array<FilterTreeSlot,4000/sizeof(FilterTreeSlot)> FilterTreeSlots;

struct FilterLeafSlot
	{
	typedef long Key;
	Key key;
	short count;
	FilterLeafSlot()
		{ }
	explicit FilterLeafSlot(Key k, short c = 0) : key(k), count(c)
		{ }
	void copy(const FilterLeafSlot& from)
		{
		key = from.key;
		count = from.count;
		}
	bool operator<(const FilterLeafSlot& y) const
		{ return key < y.key; }
	bool operator==(const FilterLeafSlot& y) const
		{ return key == y.key; }
	};
typedef Array<FilterLeafSlot,4000/sizeof(FilterLeafSlot)> FilterLeafSlots;
typedef Btree<FilterLeafSlot,FilterTreeSlot,FilterLeafSlots,FilterTreeSlots,TempDest> Filter;

struct Cmp
	{
	gcstring ident;
	int op;
	gcstring value;
	Lisp<gcstring> values;
	Cmp()
		{ }
	Cmp(const gcstring& i, int o, const gcstring& v) : ident(i), op(o), value(v)
		{ }
	Cmp(const gcstring& i, int o, const Lisp<gcstring>& v) : ident(i), op(o), values(v)
		{ }
	bool operator<(const Cmp& y) const
		{ return ident < y.ident; }
	bool operator>(const Cmp& y) const
		{ return ident > y.ident; }
	bool operator==(const Cmp& y) const
		{ return ident == y.ident && op == y.op && value == y.value; }
	bool operator!=(const Cmp& y) const
		{ return ident != y.ident || op != y.op || value != y.value; }
	};

Ostream& operator<<(Ostream& os, const Cmp& cmp)
	{
	os << cmp.ident;
	if (cmp.op == K_IN)
		os << " in " << cmp.values;
	else
		os << opcodes[cmp.op] << cmp.value;
	return os;
	}

typedef HashMap<QIndex,double,HashFirst<QIndex>,EqFirst<QIndex> > Ifracs;

class Select : public Query1
	{
public:
	Select(Query* s, Expr* e);
	void out(Ostream& os) const;
	Fields columns()
		{ return source->columns(); }
	Indexes keys()
		{ return source->keys(); }
	Indexes indexes()
		{ return source->indexes(); }
	Query* transform();
	double optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze);
	Lisp<Fixed> fixed() const;
	// estimated result sizes
	double nrecords()
		{ verify(nrecs >= 0); return nrecs; }
	// iteration
	Header header()
		{ return source->header(); }
	void select(const Fields& index, const Record& from, const Record& to);
	void rewind();
	Row get(Dir dir);

	void set_transaction(int t)
		{ tran = t; Query1::set_transaction(t); }

	bool output(const Record& r)
		{ return source->output(r); }

	void close()
		{
		if (n_in > 100 && n_in > 100 * n_out)
			TRACE(SLOWQUERY, n_in << "->" << n_out << "  " << this);
		if (f)
			f->free();
		Query1::close();
		}

	Indexes possible;
	HashMap<Field,Iselect> isels;
	Indexes filter;
protected: // not private so tests can subclass and override
	Select() : Query1(NULL), // for tests
		first(true), rewound(true), newrange(true), conflicting(false), 
		f(0), tran(-1), n_in(0), n_out(0), fixdone(false)
		{ }
	And* expr;
	bool first; // used and then reset by optimize, then used again by next
	bool rewound;
	bool newrange;
	bool conflicting;
	Table* tbl;
	Indexes theindexes; // const
	double nrecs;
	HashMap<Field,double> ffracs;
	Ifracs ifracs;
	Fields prior_needs;
	Fields select_needs;
	QIndex primary;
	Lisp<Keyrange> ranges;
	int range_i;
	Keyrange sel;
	Filter *f;
	Header hdr;
	int tran;
	int n_in;
	int n_out;

	void optimize_setup();
	Lisp<Cmp> extract_cmps();
	void cmps_to_isels(Lisp<Cmp> cmps);
	void identify_possible();
	void calc_field_fracs();
	double field_frac(const Field& field);
	void calc_index_fracs();
	
	double choose_primary(const QIndex& index);
	double primarycost(const Fields& primary);
	double choose_filter(double primary_cost);
	double costwith(const Indexes& filter, double primary_index_cost);
	double datafrac(Indexes indexes);
	
	bool matches(Row& row);
	bool matches(Fields idx, const Record& key);
	Iselects iselects(const Fields& idx);
	void iterate_setup();
	static Lisp<Keyrange> selects(const Fields& index, const Iselects& iselects);
	bool distribute(Query2* q2);
	Expr* project(Query* q);
	void convert_select(const Fields& index, const Record& from, const Record& to);
	
	mutable bool fixdone;
	mutable Lisp<Fixed> fix;
	Fields required_index;
	Fields source_index; // may have extra stuff on the end, or be missing fields that are fixed
	};

gcstring fieldmax(1, "\x7f");

Query* Query::make_select(Query* s, Expr* e)
	{
	return new Select(s, e);
	}

Select::Select(Query* s, Expr* e) :
	Query1(s), first(true), rewound(true), newrange(true), conflicting(false), nrecs(-1), 
	f(0), tran(-1), n_in(0), n_out(0), fixdone(false)
	{
	e = e->fold();
	expr = dynamic_cast<And*>(e);
	if (! expr)
		expr = new And(Lisp<Expr*>(e));
	if (! subset(source->columns(), expr->fields()))
		except("select: nonexistent columns: " << 
			difference(expr->fields(), source->columns()));
	}

void Select::out(Ostream& os) const
	{
	os << *source << " WHERE";
	if (conflicting)
		{ os << " nothing"; return ; }
	if (! nil(source_index))
		os << "^" << source_index;
	if (! nil(filter))
		os << "%" << filter;
	if (! nil(expr->exprs))
		os << " " << *expr;
//	os << " " << isels;
	}

//===================================================================

Query* Select::transform()
	{
	bool moved = false;
	// remove empty selects
	if (nil(expr->exprs))
		return source->transform();
	// combine selects
	if (Select* s = dynamic_cast<Select*>(source))
		{
		expr = new And(concat(expr->exprs, s->expr->exprs));
		source = s->source;
		return transform();
		}
	// move selects before projects
	else if (Project* p = dynamic_cast<Project*>(source))
		{
		source = p->source;
		p->source = this;
		return p->transform();
		}
	// move selects before renames
	else if (Rename* r = dynamic_cast<Rename*>(source))
		{
		Expr* new_expr = expr->rename(r->to, r->from); 
		source = r->source;
		r->source = (new_expr == expr ? this : new Select(source, new_expr));
		return r->transform();
		}
	// move select before extend, unless it depends on rules
	else if (Extend* extend = dynamic_cast<Extend*>(source))
		{
		Lisp<Expr*> src1, rest;
		for (Lisp<Expr*> exprs(expr->exprs); ! nil(exprs); ++exprs)
			{
			Expr* e = *exprs;
			if (nil(intersect(extend->rules, e->fields())))
				src1.push(e->replace(extend->flds, extend->exprs));
			else
				rest.push(e);
			}
		if (! nil(src1))
			extend->source = new Select(extend->source, new And(src1.reverse()));
		if (! nil(rest))
			expr = new And(rest.reverse());
		else
			moved = true;
		}
	// split select before & after summarize
	else if (Summarize* summarize = dynamic_cast<Summarize*>(source))
		{
		Fields flds1 = summarize->source->columns();
		Lisp<Expr*> src1, rest;
		for (Lisp<Expr*> exprs(expr->exprs); ! nil(exprs); ++exprs)
			{
			Expr* e = *exprs;
			if (subset(flds1, e->fields()))
				src1.push(e);
			else
				rest.push(e);
			}
		if (! nil(src1))
			summarize->source = new Select(summarize->source, new And(src1.reverse()));
		if (! nil(rest))
			expr = new And(rest.reverse());
		else
			moved = true;
		}	
	// distribute select over intersect
	else if (Intersect* q = dynamic_cast<Intersect*>(source))
		{
		q->source = new Select(q->source, expr);
		q->source2 = new Select(q->source2, expr);
		moved = true;
		}
	// distribute select over difference
	else if (Difference* q = dynamic_cast<Difference*>(source))
		{
		q->source = new Select(q->source, expr);
		q->source2 = new Select(q->source2, project(q->source2));
		moved = true;
		}
	// distribute select over union
	else if (Union* q = dynamic_cast<Union*>(source))
		{
		q->source = new Select(q->source, project(q->source));
		q->source2 = new Select(q->source2, project(q->source2));
		moved = true;
		}
	// split select over product
	else if (Product* x = dynamic_cast<Product*>(source))
		{
		moved = distribute(x);
		}
	// split select over leftjoin (left side only)
	else if (LeftJoin* j = dynamic_cast<LeftJoin*>(source))
		{
		Fields flds1 = j->source->columns();
		Lisp<Expr*> common, src1;
		for (Lisp<Expr*> e(expr->exprs); ! nil(e); ++e)
			{
			Expr* n = *e;
			if (subset(flds1, n->fields()))
				src1.push(n);
			else
				common.push(n);
			}
		if (! nil(src1))
			j->source = new Select(j->source, new And(src1.reverse()));
		if (! nil(common))
			expr = new And(common.reverse());
		else
			moved = true;
		}
	// NOTE: must check for LeftJoin before Join since it's derived
	// split select over join
	else if (Join* j = dynamic_cast<Join*>(source))
		{
		moved = distribute(j);
		}
	source = source->transform();
	return moved ? source : this;
	}

bool Select::distribute(Query2* q2)
	{
	Fields flds1 = q2->source->columns();
	Fields flds2 = q2->source2->columns();
	Lisp<Expr*> common, src1, src2;
	for (Lisp<Expr*> e(expr->exprs); ! nil(e); ++e)
		{
		Expr* n = *e;
		bool used = false;
		if (subset(flds1, n->fields()))
			{ src1.push(n); used = true; }
		if (subset(flds2, n->fields()))
			{ src2.push(n); used = true; }
		if (! used)
			common.push(n);
		}
	if (! nil(src1))
		q2->source = new Select(q2->source, new And(src1.reverse()));
	if (! nil(src2))
		q2->source2 = new Select(q2->source2, new And(src2.reverse()));
	if (! nil(common))
		expr = new And(common.reverse());
	else
		return true;
	return false;
	}

Expr* Select::project(Query* q)
	{
	static Expr* empty_string = make_constant(SuEmptyString);
	Fields srcflds = q->columns();
	Fields exprflds = expr->fields();
	Fields missing = difference(exprflds, srcflds);
	return expr->replace(missing, lispn(empty_string, missing.size()));
	}

//===================================================================

double Select::optimize2(const Fields& index, const Fields& needs, 
	const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	if (first)
		{
		prior_needs = needs;
		select_needs = expr->fields();
		tbl = dynamic_cast<Table*>(source);
		}
	if (! tbl || // source isnt a Table
		nil(*tbl->indexes())) // empty key() singleton - index irrelevant
		{
		first = freeze ? true : false;
		required_index = source_index = index;
		double cost = source->optimize(index, set_union(needs, select_needs), 
			set_union(firstneeds, select_needs), is_cursor, freeze);
		nrecs = source->nrecords();
		return cost;
		}

	LOG("Select::optimize " << tbl->table << (freeze ? " FREEZE" : "") << 
		" index " << index << ", needs " << needs);
	LOG("exprs " << expr);
	if (first)
		{
		first = false;
		optimize_setup();
		}

	if (conflicting)
		return 0;
	
	primary = Fields();
	filter = Indexes();

	double cost = choose_primary(index);
	if (nil(primary))
		return IMPOSSIBLE;
	
	if (! is_cursor)
		cost = choose_filter(cost);

	if (! freeze)
		return cost;

	required_index = index;
	source_index = primary;
	tbl->select_index(source_index);
	
	first = true; // for iteration
	return cost;
	}

Lisp<Fixed> combine(Lisp<Fixed> fixed1, const Lisp<Fixed>& fixed2);

void Select::optimize_setup()
	{
	fixed(); // calc before altering expr
	
	// the code depends on using the same indexes throughout (compares pointers)
	theindexes = tbl->indexes();

	Lisp<Cmp> cmps = extract_cmps(); // WARNING: modifies expr
	cmps_to_isels(cmps);
	if (conflicting)
		return ;
	identify_possible();
	calc_field_fracs();
	calc_index_fracs();
	
	// TODO: should be frac of complete select, not just indexes
	nrecs = datafrac(theindexes) * tbl->nrecords();
	}

Lisp<Fixed> Select::fixed() const
	{
	if (fixdone)
		return fix;
	fixdone = true;
	Fields fields = source->columns();
	for (Lisp<Expr*> exprs(expr->exprs); ! nil(exprs); ++exprs)
		{
		if ((*exprs)->is_term(fields))
			{
			// TODO: handle IN
			BinOp* binop = dynamic_cast<BinOp*>(*exprs);
			if (binop && binop->op == I_IS)
				{
				gcstring field = dynamic_cast<Identifier*>(binop->left)->ident;
				Value value = dynamic_cast<Constant*>(binop->right)->value;
				fix.push(Fixed(field, value));
				}
			}
		}
	return fix = combine(fix, source->fixed());
	}

Lisp<Cmp> Select::extract_cmps()
	{
	Lisp<Cmp> cmps;
	Lisp<Expr*> newexprs;
	Fields fields = theDB()->get_fields(tbl->table);
	for (Lisp<Expr*> exprs(expr->exprs); ! nil(exprs); ++exprs)
		{
		if ((*exprs)->term(fields))
			{
			if (In* in = dynamic_cast<In*>(*exprs))
				{
				Identifier* id = dynamic_cast<Identifier*>(in->expr);
				verify(id);
				cmps.push(Cmp(id->ident, K_IN, in->packed));
				continue ;
				}
			else
				{
				BinOp* binop = dynamic_cast<BinOp*>(*exprs);
				if (binop && binop->op != I_ISNT)
					{
					gcstring field = dynamic_cast<Identifier*>(binop->left)->ident;
					gcstring value = dynamic_cast<Constant*>(binop->right)->packed;
					cmps.push(Cmp(field, binop->op, value));
					continue ;
					}
				}
			}
		/* this isn't valid till they're actually used in 
		if (BinOp* binop = dynamic_cast<BinOp*>(*exprs))
			{
			verify(binop->op == I_MATCH || binop->op == I_MATCHNOT || binop->op == I_ISNT);
			if (binop->left->isfield(fields) && dynamic_cast<Constant*>(binop->right))
				{
				gcstring field = dynamic_cast<Identifier*>(binop->left)->ident;
				ffracs[field] = .5;
				}
			} */
		newexprs.push(*exprs);
		}
	expr = new And(newexprs);
	LOG("exprs " << *expr);
	LOG("cmps: " << cmps);
	return cmps;
	}

void Select::cmps_to_isels(Lisp<Cmp> cmps)
	{
	// combine cmps into one isel per field
	cmps.sort();
	Iselect isel;
	while (! nil(cmps))
		{
		Cmp& cmp = *cmps;
		Iselect r;
		switch (cmp.op)
			{
		case I_IS :		r.org.x = r.end.x = cmp.value; isel.and_with(r); break ;
		case I_LT :		r.end.x = cmp.value; r.end.d = -1; isel.and_with(r); break ;
		case I_LTE :	r.end.x = cmp.value; isel.and_with(r); break ;
		case I_GT :		r.org.x = cmp.value; r.org.d = +1; isel.and_with(r); break ;
		case I_GTE :	r.org.x = cmp.value; isel.and_with(r); break ;
		case K_IN :		r.values = cmp.values; r.type = ISEL_VALUES; isel.and_with(r); break ;
		default :		error("can't happen");
			}

		if (nil(++cmps) || (*cmps).ident != cmp.ident)
			{
			// end of group
			if (isel.none())
				{ nrecs = 0; conflicting = true; return ; }
			isel.values.sort();
			if (! isel.all())
				isels[cmp.ident] = isel;
			isel = Iselect();
			}
		}
	LOG("isels " << isels);
	}

void Select::identify_possible()
	{
	// possible = indexes with isels
	Indexes idxs(theindexes);
	for (; ! nil(idxs); ++idxs)
		{
		QIndex idx(*idxs);
		for (Fields flds(idx); ! nil(flds); ++flds)
			{
			if (isels.find(*flds))
				{
				possible.push(idx);
				break ;
				}
			}
		}
	LOG("possible " << possible);
	}

void Select::calc_field_fracs()
	{
	// ffracs = fraction selected by each field with isel
	for (Indexes idxs = possible; ! nil(idxs); ++idxs)
		{
		QIndex idx(*idxs);
		for (Fields flds(idx); ! nil(flds); ++flds)
			{
			Field fld(*flds);
			if (isels.find(fld) && ! ffracs.find(fld))
				{
				ffracs[fld] = field_frac(fld);
				}
			}
		}
	LOG("ffracs " << ffracs);
	}

// fraction of data selected by the isel on a field
double Select::field_frac(const Field& field)
	{
	Fields best_index;
	int best_size = INT_MAX;
	// look for smallest index starting with field
	for (Indexes idxs(theindexes); ! nil(idxs); ++idxs)
		{
		if (**idxs == field && tbl->indexsize(*idxs) < best_size)
			{
			best_index = *idxs;
			best_size = tbl->indexsize(*idxs);
			}
		}
	if (nil(best_index))
		return .5;
	Iselect fsel = *isels.find(field);
	double frac = tbl->iselsize(best_index, Iselects(fsel));
	LOG("field_frac " << best_index << " " << fsel << " = " << frac);
	return frac;
	}

void Select::calc_index_fracs()
	{
	// ifracs = fraction selected from each index
	for (Indexes idxs = theindexes; ! nil(idxs); ++idxs)
		{
		QIndex idx = *idxs;
		Iselects multisel = iselects(idx);
		if (nil(multisel))
			ifracs[idx] = 1;
		else
			ifracs[idx] = tbl->iselsize(idx, multisel);
		}
	LOG("ifracs " << ifracs);
	}
	
double Select::choose_primary(const QIndex& index)
	{
	// find index that satisfies required index with least cost
	LOG("choose_primary for " << index << ", fixed = " << fixed());
	double best_cost = IMPOSSIBLE;
	for (Indexes idxs(theindexes); ! nil(idxs); ++idxs)
		{
		if (! prefixed(*idxs, index, fixed()))
			continue ;
		double cost = primarycost(*idxs);
		LOG("primary " << *idxs << " cost " << cost);
		if (cost < best_cost)
			{
			primary = *idxs;
			best_cost = cost;
			}
		}
	LOG("best primary " << primary << " cost " << best_cost << "\n");
	return best_cost;
	}

double Select::primarycost(const Fields& primary)
	{
	double index_read_cost = ifracs[primary] * tbl->indexsize(primary);
	
	double data_frac = subset(primary, select_needs)
		? subset(primary, prior_needs) ? 0 : .5 * datafrac(Indexes(primary))
		: datafrac(Indexes(primary));
	
	double data_read_cost = data_frac * tbl->totalsize();
	
	LOG("primarycost(" << primary << ") index_read_cost " << index_read_cost << 
		", data_frac " << data_frac << ", data_read_cost " << data_read_cost);
	return index_read_cost + data_read_cost;
	}

double Select::choose_filter(double primary_cost)
	{
	Indexes available = ::erase(possible, primary);
	if (nil(available))
		return primary_cost;
	const double primary_index_cost = ifracs[primary] * tbl->indexsize(primary);
	double best_cost = primary_cost;
	filter = Indexes();
	while (true)
		{
		Fields best_filter;
		for (Indexes idxs = available; ! nil(idxs); ++idxs)
			{
			if (member(filter, *idxs))
				continue ;
			double cost = costwith(cons(*idxs, filter), primary_index_cost);
			if (cost < best_cost)
				{
				best_cost = cost;
				best_filter = *idxs;
				}
			}
		if (nil(best_filter))
			break ; // can't reduce cost by adding another filter
		filter.append(best_filter);
		}
	return best_cost;
	}

const int FILTER_KEYSIZE = 10;

static bool includes(const Indexes& indexes, Fields fields);

// compute the cost of using the current primary with this filter
double Select::costwith(const Indexes& filter, double primary_index_cost)
	{
	LOG("cost with: " << primary << " + " << filter);
	Indexes all(cons(primary, filter));
	double data_frac = includes(all, select_needs)
		? subset(primary, prior_needs) ? 0 : .5 * datafrac(all)
		: datafrac(all);
	
	// approximate filter cost independent of order of filters
	double filter_cost = 0;
	for (Indexes f(filter); ! nil(f); ++f)
		{
		double n = tbl->nrecords() * ifracs[*f];
		filter_cost +=
			n * tbl->keysize(*f) +	// read cost
			n * FILTER_KEYSIZE * WRITE_FACTOR; // write cost
		}
	
	LOG(" = " << data_frac << " * " << tbl->totalsize() <<
		" + " << primary_index_cost << " + " << filter_cost <<
		" = " << data_frac * tbl->totalsize() + primary_index_cost + filter_cost);
	return data_frac * tbl->totalsize() + primary_index_cost + filter_cost;
	}

// determine whether a set of indexes includes a set of fields
// like subset, but set of indexes is list of lists
static bool includes(const Indexes& indexes, Fields fields)
	{
	for (; ! nil(fields); ++fields)
		{
		gcstring field(*fields);
		for (Indexes set(indexes); ! nil(set); ++set)
			for (QIndex idx(*set); ! nil(idx); ++idx)
				if (*idx == field)
					goto found;
		return false; // not found
	found: ;
		}
	return true;
	}

// compute the fraction of data selected by a set of indexes
double Select::datafrac(Indexes indexes)
	{
 	// take the union of all the index fields
	// to ensure you don't use a field more than once
	Fields flds;
	for (Indexes idxs = indexes; ! nil(idxs); ++idxs)
		flds = set_union(flds, *idxs);

	// frac = product of frac of each field
	double frac = 1;
	for (; ! nil(flds); ++flds)
		if (double* f = ffracs.find(*flds))
			frac *= *f;
	LOG("datafrac " << indexes << " = " << frac);
	return frac;
	}

//===================================================================

void Select::select(const Fields& index, const Record& from, const Record& to)
	{
	LOG("select " << index << " " << from << " => " << to);
	// asserteq(index, required_index); // not sure why this fails
	if (conflicting)
		{
		sel.org = keymax;
		sel.end = keymin;
		}
	else if (prefix(source_index, index))
		{
		sel.org = from;
		sel.end = to;
		}
	else
		{
		convert_select(index, from, to);
		LOG("converted to " << source_index << " " << sel.org << " => " << sel.end);
		}
	rewound = true;
	}

static Value getfixed(Lisp<Fixed> f, const gcstring& field)
	{
	for (; ! nil(f); ++f)
		if (f->field == field && f->values.size() == 1)
			return *f->values;
	return Value();
	}

void Select::convert_select(const Fields& index, const Record& from, const Record& to)
	{
	// TODO: could optimize for case where from == to
	if (from == keymin && to == keymax)
		{
		sel.org = keymin;
		sel.end = keymax;
		return ;
		}
	Record newfrom, newto;
	Fields si(source_index);
	Fields ri(index);
	Value fixval;
	while (! nil(ri))
		{
		if (! nil(si) && *si == *ri)
			{
			int i = search(index, *ri);
			newfrom.addraw(from.getraw(i));
			newto.addraw(to.getraw(i));
			++si;
			++ri;
			}
		else if (! nil(si) && (fixval = getfixed(fix, *si)))
			{
			newfrom.addval(fixval);
			newto.addval(fixval);
			++si; 
			}
		else if ((fixval = getfixed(fix, *ri)))
			{
			int i = search(index, *ri);
			Value f = from.getval(i);
			Value t = to.getval(i);
			if (fixval < f || t < fixval)
				{
				// select doesn't match fixed so empty select
				sel.org = keymax;
				sel.end = keymin;
				return ;
				}
			++ri;
			}
		else
			unreachable(); 
		}
	if (from.getraw(from.size() - 1) == fieldmax)
		newfrom.addraw(fieldmax);
	if (to.getraw(to.size() - 1) == fieldmax)
		newto.addraw(fieldmax);
	sel.org = newfrom;
	sel.end = newto;
	}

//===================================================================

void Select::rewind()
	{
	rewound = true;
	source->rewind();
	}

static Keyrange intersect(const Keyrange& r1, const Keyrange& r2);

// TODO: use more efficient type for ranges
Row Select::get(Dir dir)
	{
	if (first)
		{
		first = false;
		iterate_setup();
		}
	if (rewound)
		{
		rewound = false;
		newrange = true;
		range_i = (dir == NEXT ? -1 : ranges.size()); // allow for increment below
		}
	for (;;)
		{
		if (newrange)
			{
			Keyrange range;
			do
				{
				range_i += (dir == NEXT ? 1 : -1);
				if (dir == NEXT ? range_i >= ranges.size() : range_i < 0)
					return Eof;
				range = intersect(sel, ranges[range_i]);
				}
				while (! range);
			source->select(source_index, range.org, range.end);
			newrange = false;
			}
		Row row;
		do
			{
			row = source->get(dir);
			++n_in;
			}
			while (row != Eof && ! matches(row));
		if (row != Eof)
			{
			++n_out;
			return row;
			}
		newrange = true;
		}
	}

void Select::iterate_setup()
	{
	// process filters
	if (! nil(filter))
		{
		f = new Filter(new TempDest);
		Indexes idxs = filter;
		// process first filter
		Fields ix = *idxs++;
		tbl->set_index(ix);
		Lisp<Keyrange> ranges = selects(ix, iselects(ix));
		while (! nil(ranges))
			{
			Keyrange& range = *ranges++;
			LOG("filter range: " << range.org << " => " << range.end);
			for (source->select(ix, range.org, range.end); Eof != source->get(NEXT); )
				if (matches(ix, tbl->iter->key))
					f->insert(FilterLeafSlot(tbl->iter->adr(), 1));
			}
		// process additional filters
		while (! nil(idxs))
			{
			Fields ix = *idxs++;
			tbl->set_index(ix);
			Lisp<Keyrange> ranges = selects(ix, iselects(ix));
			while (! nil(ranges))
				{
				Keyrange& range = *ranges++;
				LOG("filter range: " << range.org << " => " << range.end);
				for (source->select(ix, range.org, range.end); Eof != source->get(NEXT); )
					if (matches(ix, tbl->iter->key))
						f->increment(tbl->iter->adr());
				}
			}
		tbl->set_index(source_index); // restore primary index
		
		// remove filter isels - no longer needed
		for (idxs = filter; ! nil(idxs); ++idxs)
			for (Fields flds(*idxs); ! nil(flds); ++flds)
				isels.erase(*flds);
		}

	hdr = source->header();
	ranges = selects(source_index, iselects(source_index));
	LOG("ranges " << ranges);
	}

Lisp<Keyrange> Select::selects(const Fields& index, const Iselects& iselects)
	{
	Iselects isels;

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
			Lisp<Keyrange> result;
			for (Lisp<gcstring> values = isel.values; ! nil(values); ++values)
				{
				isel.values = Lisp<gcstring>(*values);
				result = concat(result, selects(index, iselects));
				}
			isel.values = save;
			return result;
			}
		}

	// now build the keys
	int i = 0;
	Record org;
	Record end;
	if (nil(iselects))
		end.addmax();
	for (isels = iselects; ! nil(isels); ++isels, ++i)
		{
		Iselect& isel = *isels;
		verify(! isel.none());
		if (isel.one())
			{
			if (isel.type == ISEL_RANGE)
				{
				// range
				org.addraw(isel.org.x);
				end.addraw(isel.org.x);
				}
			else
				{
				// in set
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
	LOG("select " << org << " -> " << end);
	return lisp(Keyrange(org, end));
	}

bool Select::matches(Fields idx, const Record& key)
	{
	for (int i = 0; ! nil(idx) && ! nil(key); ++idx, ++i)
		if (! isels[*idx].matches(key.getraw(i)))
			return false;
	return true;
	}

static Keyrange intersect(const Keyrange& r1, const Keyrange& r2)
	{
	Keyrange result(
		(r1.org < r2.org ? r2.org : r1.org),
		(r1.end < r2.end ? r1.end : r2.end));	
	LOG("intersect " << r1 << " and " << r2 << " => " << result);
	return result;
	}

Iselects Select::iselects(const Fields& idx)
	{
	Iselects multisel;
	for (Fields flds(idx); ! nil(flds); ++flds)
		{
		Iselect* r;
		Field field(*flds);
		if (0 == (r = isels.find(field)))
			break ;
		multisel.push(*r);
		if (r->type == ISEL_RANGE && ! r->one())
			break ; // can't add anything after range
		}
	return multisel.reverse();
	}

bool Select::matches(Row& row)
	{
	// first check against filter
	if (f)
		{
		Filter::iterator iter = f->find(tbl->iter->adr());
		if (iter == f->end() || iter->count != size(filter))
			return false;
		}
	// then check against isels
	// TODO: check keys before data (every other one)
	for (HashMap<Field,Iselect>::iterator iter = isels.begin(); iter != isels.end(); ++iter)
		{
		Iselect& isel = iter->val;
		gcstring value = row.getraw(hdr, iter->key);
		if (! isel.matches(value))
			return false;
		}
	// finally check remaining expressions
	row.set_transaction(tran);
	return expr->eval(hdr, row) == SuTrue;
	}

// Iselect ==========================================================

#define show(s) ((s) == fieldmax ? Value(new SuString("MAX")) : unpack(s))

Ostream& operator<<(Ostream& os, const Iselect& r)
	{
	if (r.type == ISEL_VALUES)
		return os << "in (" << r.values << ")";
	return os << (r.org.d == 0 ? "[" : "]") <<
		show(r.org.x) << "," << show(r.end.x) <<
		(r.end.d == 0 ? "]" : "[");
	}

void Iselect::and_with(const Iselect& r)
	{
	if (type == ISEL_RANGE && r.type == ISEL_RANGE)
		{
		// both ranges
		if (r.org > org)
			org = r.org;
		if (r.end < end)
			end = r.end;
		}
	else if (type == ISEL_VALUES && r.type == ISEL_VALUES)
		{
		// both sets
		values = intersect(values, r.values);
		}
	else
		{
		// set and range
		Lisp<gcstring> v = (type == ISEL_RANGE ? r.values : values);
		if (type == ISEL_VALUES)
			{ org = r.org; end = r.end; }
		values = Lisp<gcstring>();
		for (; ! nil(v); ++v)
			if (inrange(*v))
				values.push(*v);
		values.reverse();
		org.x = fieldmax; end.x = "";	// empty range
		type = ISEL_VALUES;
		}
	}

bool Iselect::matches(const gcstring& value)
	{
	if (type == ISEL_RANGE)
		return inrange(value);
	else
		return member(values, value);
	}

bool Iselect::inrange(const gcstring& x)
	{
	return 
		(org.d == 0 ? (org.x <= x) : (org.x < x))
		&&
		(end.d == 0 ? (x <= end.x) : (x < end.x));
	}

//===================================================================

static Record makemax()
	{
	Record max;
	max.addmax();
	return max;
	}

Record keymin;
Record keymax = makemax();

//===================================================================

#include "testing.h"

class TestSelect : public Select
	{
	friend class test_qselect;
	};

class TestTable : public Table
	{
	friend class test_qselect;
public:
	double nrecords()
		{ return 1000; }
	Fields columns()
		{ return lisp(gcstring("a"), gcstring("b"), gcstring("c")); }
	int keysize(const Fields& index)
		{ return size(index) * 10; }
	int indexsize(const Fields& index)
		{ return size(index) * 10000; }
	int totalsize()
		{ return 100000; }
	float iselsize(const Fields& index, const Iselects& isels)
		{
		iselsize_index = index;
		return (float) .1;
		}
	Fields iselsize_index;
	};

#define assertfeq(x, y)		verify(fabs((x) - (y)) < .01)

class test_qselect : public Tests
	{
	TEST(0, choose_primary)
		{
		TestSelect ts;
		ts.fixdone = true;
		TestTable tbl;
		ts.source = ts.tbl = &tbl;
		Fields index_a = lisp(gcstring("a"));
		Fields index_b = lisp(gcstring("b"));
		Fields index_a_b = lisp(gcstring("a"), gcstring("b"));
		
		ts.choose_primary(Fields());
		verify(nil(ts.primary));
		
		ts.theindexes = lisp(index_a_b);
		ts.ifracs[index_a_b] = .12;
		ts.ffracs["a"] = .3;
		ts.ffracs["b"] = .4;
		ts.choose_primary(index_b);
		verify(nil(ts.primary));
		ts.choose_primary(Fields());
		asserteq(index_a_b, ts.primary);
		ts.choose_primary(index_a);
		asserteq(index_a_b, ts.primary);
		ts.choose_primary(index_a_b);
		asserteq(index_a_b, ts.primary);
		
		ts.theindexes = lisp(index_a, index_a_b, index_b);
		ts.ifracs[index_a] = .3;
		ts.ifracs[index_b] = .4;
		ts.choose_primary(Fields());
		asserteq(index_a_b, ts.primary);
		ts.choose_primary(index_a);
		asserteq(index_a_b, ts.primary);
		ts.choose_primary(index_a_b);
		asserteq(index_a_b, ts.primary);
		ts.choose_primary(index_b);
		asserteq(index_b, ts.primary);
		}
	TEST(1, frac)
		{
		TestSelect ts;
		ts.fixdone = true;
		TestTable tbl;
		ts.source = ts.tbl = &tbl;
		assertfeq(ts.field_frac("a"), .5);
		verify(nil(tbl.iselsize_index));
		ts.isels["a"] = Iselect();
		ts.isels["b"] = Iselect();
		Fields index_a = lisp(gcstring("a"));
		Fields index_b = lisp(gcstring("b"));
		Fields index_a_c = lisp(gcstring("a"), gcstring("c"));
		ts.theindexes = lisp(index_b, index_a_c, index_a);
		assertfeq(ts.field_frac("a"), .1);
		asserteq(tbl.iselsize_index, index_a);
		assertfeq(ts.field_frac("b"), .1);
		asserteq(tbl.iselsize_index, index_b);
		assertfeq(ts.field_frac("c"), .5);
		}
	};
REGISTER(test_qselect);

