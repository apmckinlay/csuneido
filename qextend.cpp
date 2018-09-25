// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "qextend.h"
#include "qexpr.h"

Query* Query::make_extend(Query* source, const Fields& f, Lisp<Expr*> e) {
	return new Extend(source, f, e);
}

Extend::Extend(Query* source, const Fields& f, Lisp<Expr*> e)
	: Query1(source), flds(f), exprs(e), first(true), fixdone(false) {
	fold();
	init();
	check_dependencies();
}

void Extend::fold() {
	for (auto e = exprs; !nil(e); ++e)
		if (*e)
			*e = (*e)->fold();
}

void Extend::check_dependencies() {
	Fields avail = source->columns();
	Lisp<Expr*> e = exprs;
	for (Fields f = flds; !nil(f); ++f, ++e) {
		if (*e) {
			Fields ef = (*e)->fields();
			if (!subset(avail, ef))
				except("extend: invalid column(s) in expressions: "
					<< difference(ef, avail));
		}
		avail.push(*f);
	}
}

void Extend::init() {
	Fields srccols = source->columns();

	Fields dups = intersect(srccols, flds);
	if (!nil(dups))
		except("extend: column(s) already exist: " << dups);

	eflds = Fields();
	for (Lisp<Expr*> e = exprs; !nil(e); ++e)
		if (*e != NULL)
			eflds = set_union(eflds, (*e)->fields());

	// Fields avail = set_union(set_union(srccols, rules), flds);
	// Fields invalid = difference(eflds, avail);
	// if (! nil(invalid))
	//	except("extend: invalid column(s) in expressions: " << invalid);
}

void Extend::out(Ostream& os) const {
	os << *source << " EXTEND ";
	const char* sep = "";
	Lisp<Expr*> e = exprs;
	for (Fields f = flds; !nil(f); ++f, ++e) {
		os << sep << *f;
		if (*e)
			os << " = " << **e;
		sep = ", ";
	}
}

Query* Extend::transform() {
	// remove empty Extends
	if (nil(flds))
		return source->transform();
	// combine Extends
	if (Extend* e = dynamic_cast<Extend*>(source)) {
		flds = concat(e->flds, flds);
		exprs = concat(e->exprs, exprs);
		source = e->source;
		init();
		return transform();
	}
	source = source->transform();
	return this;
}

double Extend::optimize2(const Fields& index, const Fields& needs,
	const Fields& firstneeds, bool is_cursor, bool freeze) {
	if (!nil(intersect(index, flds)))
		return IMPOSSIBLE;
	// NOTE: optimize1 to bypass tempindex
	return source->optimize1(index,
		set_union(difference(eflds, flds), difference(needs, flds)),
		difference(firstneeds, flds), is_cursor, freeze);
}

void Extend::iterate_setup() {
	first = false;
	hdr = source->header() + Header(lisp(Fields(), real_fields()), flds);
}

Fields Extend::real_fields() {
	Fields real;
	Lisp<Expr*> e = exprs;
	for (Fields f = flds; !nil(f); ++f, ++e)
		if (*e)
			real.push(*f);
	return real.reverse();
}

Header Extend::header() {
	if (first)
		iterate_setup();
	return hdr;
}

Row Extend::get(Dir dir) {
	if (first)
		iterate_setup();
	Row row = source->get(dir);
	if (row == Eof)
		return Eof;
	Record r;
	Lisp<Expr*> e = exprs;
	for (Fields f = flds; !nil(f); ++f, ++e)
		if (*e) {
			// want eval to see the previously extended columns
			// have to combine every time since record's rep may change
			Value x = (*e)->eval(hdr, row + Row(lisp(Record(), r)));
			r.addval(x);
		}
	static Record emptyrec;
	return row + Row(emptyrec) + Row(r);
}

#include "qexprimp.h"

Lisp<Fixed> combine(Lisp<Fixed> fixed1, const Lisp<Fixed>& fixed2) {
	Lisp<Fixed> orig_fixed1 = fixed1;
	for (Lisp<Fixed> f2 = fixed2; !nil(f2); ++f2) {
		Lisp<Fixed> f1 = orig_fixed1;
		for (; !nil(f1); ++f1)
			if (f1->field == f2->field)
				break;
		if (nil(f1))
			fixed1.push(*f2);
	}
	return fixed1;
}

Lisp<Fixed> Extend::fixed() const {
	if (fixdone)
		return fix;
	fixdone = true;
	Lisp<Expr*> e = exprs;
	for (Fields f = flds; !nil(f); ++f, ++e)
		if (*e)
			if (Value val = (*e)->constant())
				fix.push(Fixed(*f, val));
	return fix = combine(fix, source->fixed());
}

bool Extend::output(const Record& r) {
	return source->output(r);
}

bool Extend::has_rules() {
	return exprs.member(NULL);
}

bool Extend::need_rule(Fields fields) {
	for (Fields f = fields; !nil(f); ++f)
		if (need_rule(*f))
			return true;
	return false;
}

bool Extend::need_rule(const gcstring& fld) {
	int i = search(flds, fld);
	if (i == -1)
		return false; // fld is not a result of extend
	Expr* expr = exprs[i];
	if (expr == NULL)
		return true; // direct dependency
	Fields exprdeps = expr->fields();
	return need_rule(exprdeps);
}
