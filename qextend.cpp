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

#include "qextend.h"
#include "qexpr.h"

Query* Query::make_extend(Query* source, const Fields& f, Lisp<Expr*> e, const Fields& r)
	{
	return new Extend(source, f, e, r);
	}

Extend::Extend(Query* source, const Fields& f, Lisp<Expr*> e, const Fields& r)
	: Query1(source), flds(f), exprs(e), rules(r), first(true), srccolnums(0)
	{
	init();
	}

void Extend::init()
	{
	Fields srccols = source->columns();

	Fields dups = intersect(srccols, flds);
	if (! nil(dups))
		except("extend: column(s) already exist: " << dups);

	eflds = Fields();
	for (Lisp<Expr*> e = exprs; ! nil(e); ++e)
		eflds = set_union(eflds, (*e)->fields());

	Fields avail = set_union(set_union(srccols, rules), flds);
	Fields invalid = difference(eflds, avail);
	if (! nil(invalid))
		except("extend: invalid column(s) in expressions: " << invalid);
	}

void Extend::out(Ostream& os) const
	{
	os << *source << " EXTEND ";
	char* sep = "";
	Fields f;
	for (f = rules; ! nil(f); ++f)
		{
		os << sep << *f;
		sep = ", ";
		}
	Lisp<Expr*> e = exprs;
	for (f = flds; ! nil(f); ++f, ++e)
		{
		os << sep << *f << " = " << **e;
		sep = ", ";
		}
	}

Query* Extend::transform()
	{
	// remove empty Extends
	if (nil(flds) && nil(rules))
		return source->transform();
	// combine Extends
	if (Extend* e = dynamic_cast<Extend*>(source))
		{
		flds = concat(e->flds, flds);
		exprs = concat(e->exprs, exprs);
		rules = set_union(e->rules, rules);
		source = e->source;
		init();
		return transform();
		}
	source = source->transform();
	return this;
	}

double Extend::optimize2(const Fields& index, const Fields& needs, const Fields& firstneeds, bool is_cursor, bool freeze)
	{
	if (! nil(intersect(index, flds)))
		return IMPOSSIBLE;
	Fields extendfields = set_union(flds, rules);
	// NOTE: optimize1 to bypass tempindex
	return source->optimize1(index, 
		set_union(difference(eflds, extendfields), difference(needs, extendfields)), 
		difference(firstneeds, extendfields),
		is_cursor, freeze);
	}

void Extend::iterate_setup()
	{
	first = false;
	hdr = source->header() + Header(lisp(Fields(), flds), set_union(flds, rules));
	}

Header Extend::header()
	{
	if (first)
		iterate_setup();
	return hdr;
	}

Row Extend::get(Dir dir)
	{
	if (first)
		iterate_setup();
	Row row = source->get(dir);
	if (row == Eof)
		return Eof;
	Record r;
	Lisp<Expr*> e = exprs;
	for (Fields f = flds; ! nil(f); ++f, ++e)
		{
		// want eval to see the previously extended columns
		// have to combine every time since record's rep may change
		Value x = (*e)->eval(hdr, row + Row(lisp(Record(), r)));
		r.addval(x);
		}
	static Record emptyrec;
	return row + Row(emptyrec) + Row(r);
	}

#include "qexprimp.h"

Lisp<Fixed> combine(Lisp<Fixed> fixed1, const Lisp<Fixed>& fixed2)
	{
	Lisp<Fixed> orig_fixed1 = fixed1;
	for (Lisp<Fixed> f2 = fixed2; ! nil(f2); ++f2)
		{
		Lisp<Fixed> f1 = orig_fixed1;
		for (; ! nil(f1); ++f1)
			if (f1->field == f2->field)
				break ;
		if (nil(f1))
			fixed1.push(*f2);
		}
	return fixed1;
	}

Lisp<Fixed> Extend::fixed() const
	{
	if (fixdone)
		return fix;
	fixdone = true;
	Lisp<Expr*> e = exprs;
	for (Fields f = flds; ! nil(f); ++f, ++e)
		if (Constant* c = dynamic_cast<Constant*>(*e))
			fix.push(Fixed(*f, c->value));
	return fix = combine(fix, source->fixed());
	}

bool Extend::output(const Record& r)
	{
	return source->output(r);
	}
