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

#include "qproject.h"
#include "qexpr.h"
// these are needed by transform
#include "qrename.h"
#include "qextend.h"
#include "qdifference.h"
#include "qunion.h"
#include "qintersect.h"
#include "qproduct.h"
#include "qjoin.h"
#include "database.h"
#include "thedb.h"

Query* Query::make_project(Query* source, const Fields& f, bool allbut)
	{
	return new Project(source, f, allbut);
	}

Project::Project(Query* s, const Fields& flds_, bool allbut)
	: Query1(s), flds(flds_), strategy(NONE), first(true), idx(0), rewound(true)
	{
	Fields columns = source->columns();
	if (! subset(columns, flds))
		except("project: nonexistent column(s): " << difference(flds, columns));
	if (allbut)
		flds = difference(columns, flds);
	else
		flds = lispset(flds);

	// check if project contain candidate key
	Indexes k = source->keys();
	for (; ! nil(k); ++k)
		if (subset(flds, *k))
			break ;
	if (! nil(k))
		{
		strategy = COPY;
		includeDeps(columns);
		}
	}

void Project::includeDeps(const Fields& columns)
	{
	for (Fields f = flds; ! nil(f); ++f)
		{
		gcstring deps(*f + "_deps");
		if (columns.member(deps) && ! flds.member(deps))
			flds.append(deps);
		}
	}

void Project::out(Ostream& os) const
	{
	const char* s[] = { "", "-COPY", "-SEQ", "-LOOKUP" };
	os << *source << " PROJECT" << s[strategy];
	if (! nil(via))
	    os << "^" << via;
	os << " " << flds;
	}

Indexes Project::keys()
	{
	Indexes keys;
	for (Indexes k = source->keys(); ! nil(k); ++k)
		if (subset(flds, *k))
			keys.push(*k);
	return nil(keys) ? Indexes(flds) : keys;
	}

Indexes Project::indexes()
	{
	Indexes idxs;
	for (Indexes src = source->indexes(); ! nil(src); ++src)
		if (subset(flds, *src))
			idxs.push(*src);
	return idxs;
	}

Query* Project::transform()
	{
	bool moved = false;
	// remove projects of all fields
	if (flds == source->columns())
		return source->transform();
	// combine projects
	if (Project* p = dynamic_cast<Project*>(source))
		{
		flds = intersect(flds, p->flds);
		source = p->source;
		return transform();
		}
	// move projects before renames, renaming
	else if (Rename* r = dynamic_cast<Rename*>(source))
		{
		// remove renames not in project
		Fields new_from;
		Fields new_to;
		Fields f = r->from;
		Fields t = r->to;
		for (; ! nil(f); ++f, ++t)
			if (member(flds, *t))
				{
				new_from.push(*f);
				new_to.push(*t);
				}
		r->from = new_from.reverse();
		r->to = new_to.reverse();

		// rename fields
		Fields new_fields;
		f = flds;
		int i;
		for (; ! nil(f); ++f)
			if (-1 == (i = search(r->to, *f)))
				new_fields.push(*f);
			else
				new_fields.push(r->from[i]);
		flds = new_fields.reverse();

		source = r->source;
		r->source = this;
		return r->transform();
		}
	// move projects before extends
	else if (Extend* e = dynamic_cast<Extend*>(source))
		{
		// remove portions of extend not included in project
		Fields new_flds;
		Lisp<Expr*> new_exprs;
		Fields f = e->flds;
		Lisp<Expr*> ex = e->exprs;
		for (; ! nil(f); ++f, ++ex)
			if (member(flds, *f))
				{
				new_flds.push(*f);
				new_exprs.push(*ex);
				}
		Fields new_rules;
		Fields orig_flds = e->flds;
		e->flds = new_flds;
		Lisp<Expr*> orig_exprs = e->exprs;
		e->exprs = new_exprs;

		// project must include all fields required by extend
		// there must be no rules left
		// since we don't know what fields are required by rules
		if (! e->has_rules())
			{
			Fields eflds;
			for (ex = e->exprs; ! nil(ex); ++ex)
				eflds = set_union(eflds, (*ex)->fields());
			if (subset(flds, eflds))
				{
				// remove extend fields from project
				Fields new_fields;
				Fields f = flds;
				for (; ! nil(f); ++f)
					if (! member(e->flds, *f))
						new_fields.push(*f);
				flds = new_fields.reverse();

				source = e->source;
				e->source = this;
				e->init();
				return e->transform();
				}
			}
		e->flds = orig_flds;
		e->exprs = orig_exprs;
		}
	// distribute project over union/intersect (NOT difference)
	else if (dynamic_cast<Difference*>(source))
		{
		}
	else if (Compatible* c = dynamic_cast<Compatible*>(source))
		{
		if (c->disjoint != "" && ! member(flds, c->disjoint))
			{
			Fields flds2 = flds.copy().push(c->disjoint);
			c->source = new Project(c->source,  
				intersect(flds2, c->source->columns()));
			c->source2 = new Project(c->source2,  
				intersect(flds2, c->source2->columns()));
			}
		else
			{
			c->source = new Project(c->source, 
				intersect(flds, c->source->columns()));
			c->source2 = new Project(c->source2, 
				intersect(flds, c->source2->columns()));
			return source->transform();
			}
		}
	// split project over product/join
	else if (Product* x = dynamic_cast<Product*>(source))
		{
		x->source = new Project(x->source,
			intersect(flds, x->source->columns()));
		x->source2 = new Project(x->source2,
			intersect(flds, x->source2->columns()));
		moved = true;
		}
	else if (Join* j = dynamic_cast<Join*>(source))
		{
		if (subset(flds, j->joincols))
			{
			j->source = new Project(j->source,
				intersect(flds, j->source->columns()));
			j->source2 = new Project(j->source2,
				intersect(flds, j->source2->columns()));
			moved = true;
			}
		}
	source = source->transform();
	return moved ? source : this;
	}

double Project::optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	if (strategy == COPY)
		return source->optimize(index, needs, firstneeds, is_cursor, freeze);

	// look for index containing result key columns as prefix
	Fields best_index;
	double best_cost = IMPOSSIBLE;
	Indexes idxs = nil(index) ? source->indexes() : Indexes(index);
	Lisp<Fixed> fix = source->fixed();
	Fields fldswof = withoutFixed(flds, fix);
	for (; ! nil(idxs); ++idxs)
		{
		Fields ix = *idxs;
		if (prefix_set(withoutFixed(ix, fix), fldswof))
			{
			double cost = source->optimize1(ix, needs, firstneeds, is_cursor, false);
			if (cost < best_cost)
				{ best_cost = cost; best_index = ix; }
			}
		}
	if (nil(best_index))
		{
		if (is_cursor)
			return IMPOSSIBLE;
		if (freeze)
			strategy = LOOKUP;
		return 2 * source->optimize(index, needs, firstneeds, is_cursor, freeze); // 2* for lookups
		}
	else
		{
		if (! freeze)
			return best_cost;
		strategy = SEQUENTIAL;
		via = best_index;
		// NOTE: optimize1 to avoid tempindex
		return source->optimize1(best_index, needs, firstneeds, is_cursor, freeze);
		}
	}

Fields Project::withoutFixed(Fields fields, const Lisp<Fixed> fixed)
	{
	if (! hasFixed(fields, fixed))
		return fields;
	Fields fldswof;
	for (; ! nil(fields); ++fields)
		if (! isfixed(fixed, *fields))
			fldswof.push(*fields);
	return fldswof.reverse();
	}

bool Project::hasFixed(Fields fields, const Lisp<Fixed> fixed)
	{
	for (; ! nil(fields); ++fields)
		if (isfixed(fixed, *fields)) 
			return true;
	return false;
	}

Lisp<Fixed> Project::fixed() const
	{
	Lisp<Fixed> fixed;
	for (Lisp<Fixed> f = source->fixed(); ! nil(f); ++f)
		if (member(flds, f->field))
			fixed.push(*f);
	return fixed;
	}

Header Project::header()
	{
	return strategy == COPY
		? source->header().project(flds)
		: Header(lisp(Fields(), flds), flds);
	}

Row Project::get(Dir dir)
	{
	static Record emptyrec;

	if (first)
		{
		first = false;
		src_hdr = source->header();
		proj_hdr = src_hdr.project(flds);
		if (strategy == LOOKUP)
			{
			if (idx)
				idx->free();
			idx = new VVtree(td = new TempDest);
			indexed = false;
			}
		}
	if (strategy == COPY)
		{
		return source->get(dir);
		}
	else if (strategy == SEQUENTIAL)
		{
		if (dir == NEXT)
			{
			// output the first of each group
			// i.e. skip over rows the same as previous output
			Row row;
			do
				if (Eof == (row = source->get(NEXT)))
					return Eof;
				while (! rewound && equal(proj_hdr, row, currow));
			rewound = false;
			prevrow = currow;
			currow = row;
			// output the first row of a new group
			return Row(lisp(emptyrec, row_to_key(src_hdr, row, flds)));
			}
		else // dir == PREV
			{
			// output the last of each group
			// i.e. output when *next* record is different
			// (to get the same records as NEXT)
			if (rewound)
				prevrow = source->get(PREV);
			rewound = false;
			Row row;
			do
				{
				if (Eof == (row = prevrow))
					return Eof;
				prevrow = source->get(PREV);
				}
				while (equal(proj_hdr, row, prevrow));
			// output the last row of a group
			currow = row;
			return Row(lisp(emptyrec, row_to_key(src_hdr, row, flds)));
			}
		}
	else
		{
		verify(strategy == LOOKUP);
		if (rewound)
			{
			rewound = false;
			if (dir == PREV && ! indexed)
				{
				// pre-build the index
				Row row;
				while (Eof != (row = source->get(NEXT)))
					{
					Record key = row_to_key(src_hdr, row, flds);
					Vdata data(row.data);
					for (Lisp<Record> rs = row.data; ! nil(rs); ++rs)
						td->addref(rs->ptr());
					// insert will only succeed on first of dups
					idx->insert(VVslot(key, &data));
					}
				source->rewind();
				indexed = true;
				}
			}
		Row row;
		while (Eof != (row = source->get(dir)))
			{
			Record key = row_to_key(src_hdr, row, flds);
			VVtree::iterator iter = idx->find(key);
			if (iter == idx->end())
				{
				for (Lisp<Record> rs = row.data; ! nil(rs); ++rs)
					td->addref(rs->ptr());
				Vdata data(row.data);
				verify(idx->insert(VVslot(key, &data)));
				return Row(lisp(emptyrec, key));
				}
			else
				{
				Vdata* d = iter->data;
				Records rs;
				for (int i = d->n - 1; i >= 0; --i)
					rs.push(Record::from_int(d->r[i], theDB()->mmf));
				Row irow(rs);
				if (row == irow)
					return Row(lisp(emptyrec, key));
				}
			}
		if (dir == NEXT)
			indexed = true;
		return Eof;
		}
	}

void Project::select(const Fields& index, const Record& from, const Record& to)
	{
	source->select(index, from, to);
	if (strategy == LOOKUP && (sel.org != from || sel.end != to))
		{
		if (idx)
			idx->free();
		idx = new VVtree(td = new TempDest);
		indexed = false;
		}
	sel.org = from;
	sel.end = to;
	rewound = true;
	}

void Project::rewind()
	{
	source->rewind();
	rewound = true;
	}

bool Project::output(const Record& r)
	{
	ckmodify("output");
	return source->output(r);
	}

void Project::ckmodify(char* action)
	{
	if (strategy != COPY)
		except("project: can't " << action << ": key required");
	}

void Project::close(Query* q)
	{
	if (idx)
		idx->free();
	Query1::close(q);
	}

#include "testing.h"
#include "tempdb.h"

class test_project : public Tests
	{
	TEST(1, count)
		{
		TempDB tempdb;

		verify(count(NEXT) == count(PREV));
		}
	int count(Dir dir)
		{
		Query* q = query("columns project table join tables project nextfield");
		int tran = thedb->transaction(READONLY);
		q->set_transaction(tran);
		int n = 0;
		while (Query::Eof != q->get(dir))
			++n;
		q->close(q);
		verify(thedb->commit(tran));
		extern int tempdest_inuse;
		verify(tempdest_inuse == 0);
		return n;
		}
	TEST(2, bidir)
		{
		TempDB tempdb;

		bidir(NEXT, PREV);
		bidir(PREV, NEXT);
		}
	void bidir(Dir dir1, Dir dir2)
		{
		Query* q = query("columns project table");
		int tran = thedb->transaction(READONLY);
		q->set_transaction(tran);
		while (true)
			{
			Row row1, row2;
			if (Query::Eof == (row1 = q->get(dir1)))
				break ;
			if (Query::Eof == (row2 = q->get(dir1)))
				break ;
			verify(row1 == q->get(dir2));
			verify(row2 == q->get(dir1));
			}
		q->close(q);
		verify(thedb->commit(tran));
		extern int tempdest_inuse;
		verify(tempdest_inuse == 0);
		}
	};

REGISTER(test_project);
