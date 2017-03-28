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

#include "qexprimp.h"
#include "query.h"
#include "sunumber.h"
#include "sustring.h"
#include "qscanner.h"
#include "row.h"
#include "regexp.h"
#include "call.h"
#include "suboolean.h"

extern const char* opcodes[];

// Constant ---------------------------------------------------------

Expr* Query::make_constant(Value x)
	{
	return new Constant(x);
	}

Constant::Constant(Value x) : value(x), packed(x.packsize())
	{
	x.pack(packed.buf());
	}

void Constant::out(Ostream& os) const
	{
	os << value;
	}

Value Constant::eval(const Header&, const Row&)
	{
	return value;
	}

// Identifier -------------------------------------------------------

Expr* Query::make_identifier(const gcstring& s)
	{
	return new Identifier(s);
	}

void Identifier::out(Ostream& os) const
	{
	os << ident;
	}

bool Identifier::isfield(const Fields& fields)
	{
	return member(fields, ident);
	}

Expr* Identifier::rename(const Fields& from, const Fields& to)
	{
	int i = search(from, ident);
	return i == -1 ? this : new Identifier(to[i]);
	}

Expr* Identifier::replace(const Fields& from, const Lisp<Expr*>& to)
	{
	int i = search(from, ident);
	return i == -1 ? this : to[i];
	}

Value Identifier::eval(const Header& hdr, const Row& row)
	{
	return row.getval(hdr, ident);
	}

// UnOp -------------------------------------------------------------

Expr* Query::make_unop(short op, Expr* expr)
	{
	if (op == I_SUB)
		if (Constant* k = dynamic_cast<Constant*>(expr))
			return new Constant(-k->value);
	return new UnOp(op, expr);
	}

void UnOp::out(Ostream& os) const
	{
	os << opcodes[op] << *expr;
	}

Expr* UnOp::rename(const Fields& from, const Fields& to)
	{
	Expr* new_expr = expr->rename(from, to);
	return new_expr == expr ? this : new UnOp(op, new_expr);
	}

Expr* UnOp::replace(const Fields& from, const Lisp<Expr*>& to)
	{
	Expr* new_expr = expr->replace(from, to);
	return new_expr == expr ? this : new UnOp(op, new_expr);
	}

Expr* UnOp::fold()
	{
	Expr* new_expr = expr->fold();
	if (Constant* k = dynamic_cast<Constant*>(new_expr))
		return new Constant(eval2(k->value));
	return new_expr == expr ? this : new UnOp(op, new_expr);
	}

Value UnOp::eval(const Header& hdr, const Row& row)
	{
	Value x = expr->eval(hdr, row);
	return eval2(x);
	}

bool tobool(Value x)
	{
	return force<SuBoolean*>(x) == SuBoolean::t;
	}

Value UnOp::eval2(Value x)
	{
	switch (op)
		{
	case I_NOT :		return tobool(x) ? SuFalse : SuTrue;
	case I_BITNOT :	return ~ x.integer();
	case I_ADD :		return x;
	case I_SUB :		return -x;
	default :			error("invalid UnOp type");
		}
	}

// BinOp ------------------------------------------------------------

Expr* Query::make_binop(short op, Expr* expr1, Expr* expr2)
	{
	return new BinOp(op, expr1, expr2);
	}

void BinOp::out(Ostream& os) const
	{
	os << "(" << *left << " " << opcodes[op] << " " << *right << ")";
	}

Expr* BinOp::rename(const Fields& from, const Fields& to)
	{
	Expr* new_left = left->rename(from, to);
	Expr* new_right = right->rename(from, to);
	return new_left == left && new_right == right ? this :
		new BinOp(op, new_left, new_right);
	}

Expr* BinOp::replace(const Fields& from, const Lisp<Expr*>& to)
	{
	Expr* new_left = left->replace(from, to);
	Expr* new_right = right->replace(from, to);
	return new_left == left && new_right == right ? this :
		new BinOp(op, new_left, new_right);
	}

Expr* BinOp::fold()
	{
	Expr* new_left = left->fold();
	Expr* new_right = right->fold();
	Constant* kx = dynamic_cast<Constant*>(new_left);
	Constant* ky = dynamic_cast<Constant*>(new_right);
	if (kx && ky)
		return new Constant(eval2(kx->value, ky->value));
	return new_left == left && new_right == right ? this :
		new BinOp(op, new_left, new_right);
	}

bool BinOp::term(const Fields& fields)
	{
	return isterm = is_term(fields);
	}

bool BinOp::is_term(const Fields& fields)
	{
	// NOTE: MATCH and MATCHNOT are NOT terms
	if (! (I_IS == op || op == I_ISNT ||
		op == I_LT || op == I_LTE || op == I_GT || op == I_GTE))
		return false;
	if (left->isfield(fields) && dynamic_cast<Constant*>(right))
		return true;
	if (dynamic_cast<Constant*>(left) && right->isfield(fields))
		{
		std::swap(left, right);
		switch (op)
			{
		case I_LT : op = I_GT; break ;
		case I_LTE : op = I_GTE; break ;
		case I_GT : op = I_LT; break ;
		case I_GTE : op = I_LTE; break ;
		case I_IS : case I_ISNT : break ;
		default :
			unreachable();
			}
		return true;
		}
	return false;
	}

Value BinOp::eval(const Header& hdr, const Row& row)
	{
	if (isterm)
		{
		Identifier* id = dynamic_cast<Identifier*>(left);
		verify(id);
		gcstring field = row.getraw(hdr, id->ident);
		Constant* c = dynamic_cast<Constant*>(right);
		verify(c);
		gcstring value = c->packed;
		bool result;
		switch (op)
			{
		case I_IS :		result = (field == value); break ;
		case I_ISNT :	result = (field != value); break ;
		case I_LT :		result = (field < value); break ;
		case I_LTE :		result = ! (value < field); break ;
		case I_GT :		result = (value < field); break ;
		case I_GTE :		result = ! (field < value); break ;
		default :		unreachable();
			}
		return result ? SuTrue : SuFalse;
		}
	else
		{
		Value x = left->eval(hdr, row);
		Value y = right->eval(hdr, row);
		return eval2(x, y);
		}
	}

// make "" < all other values to match packed comparison
static bool lt(Value x, Value y)
	{
	if (y == SuEmptyString)
		return false;
	if (x == SuEmptyString)
		return true;
	return x < y;
	}

Value BinOp::eval2(Value x, Value y)
	{
	switch (op)
		{
	case I_IS :		return x == y ? SuTrue : SuFalse;
	case I_ISNT :	return x != y ? SuTrue : SuFalse;
	case I_LT :		return lt(x, y) ? SuTrue : SuFalse;
	case I_LTE :		return ! lt(y, x) ? SuTrue : SuFalse;
	case I_GT :		return lt(y, x) ? SuTrue : SuFalse;
	case I_GTE :		return ! lt(x, y) ? SuTrue : SuFalse;
	case I_ADD :		return x + y;
	case I_SUB :		return x - y;
	case I_CAT :		return new SuString(x.gcstr() + y.gcstr());
	case I_MUL :		return x * y;
	case I_DIV :		return x / y;
	case I_MOD :		return x.integer() % y.integer();
	case I_LSHIFT :	return x.integer() << y.integer();
	case I_RSHIFT :	return x.integer() >> y.integer();
	case I_BITAND :	return x.integer() & y.integer();
	case I_BITOR :	return x.integer() | y.integer();
	case I_BITXOR :	return x.integer() ^ y.integer();
	case I_MATCH :
		{
		gcstring sx = x.gcstr();
		gcstring sy = y.gcstr();
		return rx_match(sx, rx_compile(sy)) ? SuTrue : SuFalse;
		}
	case I_MATCHNOT :
		{
		gcstring sx = x.gcstr();
		gcstring sy = y.gcstr();
		return rx_match(sx, rx_compile(sy)) 	? SuFalse : SuTrue;
		}
	default :
		error("invalid BinOp type");
		}
	}

// TriOp ------------------------------------------------------------

Expr* Query::make_triop(Expr* expr, Expr* iftrue, Expr* iffalse)
	{
	return new TriOp(expr, iftrue, iffalse);
	}

void TriOp::out(Ostream& os) const
	{
	os << "(" << *expr << " ? " << *iftrue << " : " << *iffalse << ")";
	}

Expr* TriOp::rename(const Fields& from, const Fields& to)
	{
	Expr* new_expr = expr->rename(from, to);
	Expr* new_iftrue = iftrue->rename(from, to);
	Expr* new_iffalse = iffalse->rename(from, to);
	return new_expr == expr && new_iftrue == iftrue && new_iffalse == iffalse
		? this : new TriOp(new_expr, new_iftrue, new_iffalse);
	}

Expr* TriOp::replace(const Fields& from, const Lisp<Expr*>& to)
	{
	Expr* new_expr = expr->replace(from, to);
	Expr* new_iftrue = iftrue->replace(from, to);
	Expr* new_iffalse = iffalse->replace(from, to);
	return new_expr == expr && new_iftrue == iftrue && new_iffalse == iffalse
		? this : new TriOp(new_expr, new_iftrue, new_iffalse);
	}

Expr* TriOp::fold()
	{
	Expr* new_expr = expr->fold();
	Expr* new_iftrue = iftrue->fold();
	Expr* new_iffalse = iffalse->fold();
	if (Constant* kexpr = dynamic_cast<Constant*>(new_expr))
		return tobool(kexpr->value) ? new_iftrue : new_iffalse;
	return new_expr == expr && new_iftrue == iftrue && new_iffalse == iffalse
		? this : new TriOp(new_expr, new_iftrue, new_iffalse);
	}

Value TriOp::eval(const Header& hdr, const Row& row)
	{
	return tobool(expr->eval(hdr, row))
		? iftrue->eval(hdr, row)
		: iffalse->eval(hdr, row);
	}

// In ---------------------------------------------------------------

Expr* Query::make_in(Expr* expr, const Lisp<Value>& values)
	{
	return new In(expr, values);
	}

In::In(Expr* e, const Lisp<Value>& v) : expr(e), values(v), isterm(false)
	{
	for (Lisp<Value> vs = values; ! nil(vs); ++vs)
		{
		Value x = *vs;
		gcstring p(x.packsize());
		x.pack(p.buf());
		packed.push(p);
		}
	}

In::In(Expr* e, const Lisp<Value>& v, const Lisp<gcstring> p)
	: expr(e), values(v), packed(p), isterm(false)
	{ }

void In::out(Ostream& os) const
	{
	os << "(" << *expr << " in " << values << ")";
	}

bool In::term(const Fields& fields)
	{
	return isterm = is_term(fields);
	}

bool In::is_term(const Fields& fields)
	{
	return expr->isfield(fields);
	}

Expr* In::rename(const Fields& from, const Fields& to)
	{
	Expr* new_expr = expr->rename(from, to);
	return new_expr == expr ? this : new In(new_expr, values, packed);
	}

Expr* In::replace(const Fields& from, const Lisp<Expr*>& to)
	{
	Expr* new_expr = expr->replace(from, to);
	return new_expr == expr ? this : new In(new_expr, values, packed);
	}

Expr* In::fold()
	{
	Expr* new_expr = expr->fold();
	if (Constant* k = dynamic_cast<Constant*>(new_expr))
		return new Constant(eval2(k->value));
	return new_expr == expr ? this : new In(new_expr, values, packed);
	}

Value In::eval(const Header& hdr, const Row& row)
	{
	if (isterm)
		{
		Identifier* id = dynamic_cast<Identifier*>(expr);
		verify(id);
		gcstring value = row.getraw(hdr, id->ident);
		for (Lisp<gcstring> v = packed; ! nil(v); ++v)
			if (value == *v)
				return SuTrue;
		return SuFalse;
		}
	else
		{
		Value x = expr->eval(hdr, row);
		return eval2(x);
		}
	}

Value In::eval2(Value x)
	{
	for (Lisp<Value> v = values; ! nil(v); ++v)
		if (x == *v)
			return SuTrue;
	return SuFalse;
	}

// MultiOp ----------------------------------------------------------

Fields MultiOp::fields()
	{
	Fields f;
	for (Lisp<Expr*> e(exprs); ! nil(e); ++e)
		f = set_union(f, (*e)->fields());
	return f;
	}

Lisp<Expr*> MultiOp::rename_exprs(const Fields& from, const Fields& to)
	{
	bool changed = false;
	Lisp<Expr*> new_exprs;
	for (Lisp<Expr*> e(exprs); ! nil(e); ++e)
		{
		Expr* new_e = (*e)->rename(from, to);
		new_exprs.push(new_e);
		if (new_e != *e)
			changed = true;
		}
	return changed ? new_exprs.reverse() : Lisp<Expr*>();
	}

Lisp<Expr*> MultiOp::replace_exprs(const Fields& from, const Lisp<Expr*>& to)
	{
	bool changed = false;
	Lisp<Expr*> new_exprs;
	for (Lisp<Expr*> e(exprs); ! nil(e); ++e)
		{
		Expr* new_e = (*e)->replace(from, to);
		new_exprs.push(new_e);
		if (new_e != *e)
			changed = true;
		}
	return changed ? new_exprs.reverse() : Lisp<Expr*>();
	}

Lisp<Expr*> MultiOp::fold_exprs()
	{
	bool changed = false;
	Lisp<Expr*> new_exprs;
	for (Lisp<Expr*> e(exprs); ! nil(e); ++e)
		{
		Expr* new_e = (*e)->fold();
		new_exprs.push(new_e);
		if (new_e != *e)
			changed = true;
		}
	return changed ? new_exprs.reverse() : Lisp<Expr*>();
	}

// Or ---------------------------------------------------------------

Expr* Query::make_or(const Lisp<Expr*>& exprs)
	{
	return new Or(exprs);
	}

void Or::out(Ostream& os) const
	{
	if (nil(exprs))
		return ;
	Lisp<Expr*> e(exprs);
	os << "(" << **e;
	for (++e; ! nil(e); ++e)
		os << " or " << **e;
	os << ")";
	}

Expr* Or::rename(const Fields& from, const Fields& to)
	{
	Lisp<Expr*> new_exprs = rename_exprs(from, to);
	return nil(new_exprs) ? this : new Or(new_exprs);
	}

Expr* Or::replace(const Fields& from, const Lisp<Expr*>& to)
	{
	Lisp<Expr*> new_exprs = replace_exprs(from, to);
	return nil(new_exprs) ? this : new Or(new_exprs);
	}

Expr* Or::fold()
	{
	bool allfalse = true;
	Lisp<Expr*> new_exprs = fold_exprs();
	for (Lisp<Expr*> e = nil(new_exprs) ? exprs : new_exprs; ! nil(e); ++e)
		if (Constant* k = dynamic_cast<Constant*>(*e))
			{
			if (tobool(k->value))
				return new Constant(SuTrue);
			}
		else
			allfalse = false;
	if (allfalse)
		return new Constant(SuFalse);
	return nil(new_exprs) ? this : new Or(new_exprs);
	}

Value Or::eval(const Header& hdr, const Row& row)
	{
	for (Lisp<Expr*> e(exprs); ! nil(e); ++e)
		if (tobool((*e)->eval(hdr, row)))
			return SuTrue;
	return SuFalse;
	}

// And --------------------------------------------------------------

Expr* Query::make_and(const Lisp<Expr*>& exprs)
	{
	return new And(exprs);
	}

void And::out(Ostream& os) const
	{
	if (nil(exprs))
		return ;
	Lisp<Expr*> e(exprs);
	if (nil(cdr(e)))
		os << **e;
	else
		{
		os << "(" << **e;
		for (++e; ! nil(e); ++e)
			os << " and " << **e;
		os << ")";
		}
	}

Expr* And::rename(const Fields& from, const Fields& to)
	{
	Lisp<Expr*> new_exprs = rename_exprs(from, to);
	return nil(new_exprs) ? this : new And(new_exprs);
	}

Expr* And::replace(const Fields& from, const Lisp<Expr*>& to)
	{
	Lisp<Expr*> new_exprs = replace_exprs(from, to);
	return nil(new_exprs) ? this : new And(new_exprs);
	}

Expr* And::fold()
	{
	Lisp<Expr*> new_exprs = fold_exprs();
	bool alltrue = true;
	for (Lisp<Expr*> e = nil(new_exprs) ? exprs : new_exprs; ! nil(e); ++e)
		if (Constant* k = dynamic_cast<Constant*>(*e))
			{
			if (! tobool(k->value))
				return new Constant(SuFalse);
			}
		else
			alltrue = false;
	if (alltrue)
		return new Constant(SuTrue);
	return nil(new_exprs) ? this : new And(new_exprs);
	}

Value And::eval(const Header& hdr, const Row& row)
	{
	for (Lisp<Expr*> e(exprs); ! nil(e); ++e)
		if (! tobool((*e)->eval(hdr, row)))
			return SuFalse;
	return SuTrue;
	}

// FunCall ----------------------------------------------------------

Expr* Query::make_call(Expr* ob, const gcstring& fname, const Lisp<Expr*>& args)
	{
	return new FunCall(ob, fname, args);
	}

void FunCall::out(Ostream& os) const
	{
	if (ob != NULL)
		os << ob << ".";
	os << fname << "(";
	for (Lisp<Expr*> e(exprs); ! nil(e); ++e)
		{
		os << **e;
		if (! nil(cdr(e)))
			os << ", ";
		}
	os << ")";
	}

Fields FunCall::fields()
	{
	Fields f = MultiOp::fields();
	return (ob == NULL) ? f : set_union(ob->fields(), f);
	}

Expr* FunCall::rename(const Fields& from, const Fields& to)
	{
	Expr* new_ob = (ob == NULL) ? ob : ob->rename(from, to);
	Lisp<Expr*> new_exprs = rename_exprs(from, to);
	if (new_ob == ob && nil(new_exprs))
		return this;
	if (nil(new_exprs))
		new_exprs = exprs;
	return new FunCall(new_ob, fname, new_exprs);
	}

Expr* FunCall::replace(const Fields& from, const Lisp<Expr*>& to)
	{
	Expr* new_ob = (ob == NULL) ? ob : ob->replace(from, to);
	Lisp<Expr*> new_exprs = replace_exprs(from, to);
	if (new_ob == ob && nil(new_exprs))
		return this;
	if (nil(new_exprs))
		new_exprs = exprs;
	return new FunCall(new_ob, fname, new_exprs);
	}

Value FunCall::eval(const Header& hdr, const Row& row)
	{
	Lisp<Value> args;
	for (Lisp<Expr*> e(exprs); ! nil(e); ++e)
		args.push((*e)->eval(hdr, row));
	Value result;
	if (ob == NULL)
		result = call(fname.str(), args.reverse());
	else
		result = method_call(ob->eval(hdr, row), fname, args.reverse());
	if (! result)
		except("no return value from: " << fname.str());
	return result;
	}
