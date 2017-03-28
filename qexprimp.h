#pragma once
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

#include "qexpr.h"

struct Constant : public Expr
	{
	explicit Constant(Value x);
	explicit Constant(const gcstring& s) : value(0), packed(s) // used by Iselect::matches
		{ }
	void out(Ostream& os) const override;
	Fields fields() override
		{ return Fields(); }
	Value eval(const Header&, const Row&) override;
	bool operator==(const Constant& k) const
		{ return packed == k.packed; }
	Expr* rename(const Fields& from, const Fields& to) override
		{ return this; }
	Expr* replace(const Fields&, const Lisp<Expr*>&) override
		{ return this; }
	Expr* fold() override
		{ return this; }

	Value value;
	gcstring packed;
	};

struct Identifier : public Expr
	{
	explicit Identifier(const gcstring& s) : ident(s)
		{ }
	void out(Ostream& os) const override;
	Fields fields() override
		{ return Fields(ident); }
	Value eval(const Header& hdr, const Row& row) override;
	Expr* rename(const Fields& from, const Fields& to) override;
	Expr* replace(const Fields& from, const Lisp<Expr*>& to) override;
	Expr* fold() override
		{ return this; }
	bool isfield(const Fields& fields) override;

	gcstring ident;
	};

struct UnOp : public Expr
	{
	UnOp(short o, Expr* e) : op(o), expr(e)
		{ }
	void out(Ostream& os) const override;
	Fields fields() override
		{ return expr->fields(); }
	Value eval(const Header& hdr, const Row& row) override;
	Value eval2(Value x);
	Expr* rename(const Fields& from, const Fields& to) override;
	Expr* replace(const Fields& from, const Lisp<Expr*>& to) override;
	Expr* fold() override;

	short op;
	Expr* expr;
	};

struct BinOp : public Expr
	{
	BinOp(short o, Expr* l, Expr* r) : op(o), left(l), right(r)
		{ }
	void out(Ostream& os) const override;
	bool term(const Fields& fields) override;
	bool is_term(const Fields& fields) override;
	Fields fields() override
		{ return set_union(left->fields(), right->fields()); }
	Value eval(const Header& hdr, const Row& row) override;
	Value eval2(Value x, Value y);
	Expr* rename(const Fields& from, const Fields& to) override;
	Expr* replace(const Fields& from, const Lisp<Expr*>& to) override;
	Expr* fold() override;

	short op;
	Expr* left;
	Expr* right;
	bool isterm = false;
	};

struct TriOp : public Expr
	{
	TriOp(Expr* e, Expr* t, Expr* f) : expr(e), iftrue(t), iffalse(f)
		{ }
	void out(Ostream& os) const override;
	Fields fields() override
		{
		return set_union(expr->fields(),
			set_union(iftrue->fields(), iffalse->fields()));
		}
	Value eval(const Header& hdr, const Row& row) override;
	Expr* rename(const Fields& from, const Fields& to) override;
	Expr* replace(const Fields& from, const Lisp<Expr*>& to) override;
	Expr* fold() override;

	Expr* expr;
	Expr* iftrue;
	Expr* iffalse;
	};

struct In : public Expr
	{
	In(Expr* expr, const Lisp<Value>& values);
	In(Expr* expr, const Lisp<Value>& values, const Lisp<gcstring> packed);
	void out(Ostream& os) const override;
	bool term(const Fields& fields) override;
	bool is_term(const Fields& fields) override;
	Value eval(const Header& hdr, const Row& row) override;
	Value eval2(Value x);
	Fields fields() override
		{ return expr->fields(); }
	Expr* rename(const Fields& from, const Fields& to) override;
	Expr* replace(const Fields& from, const Lisp<Expr*>& to) override;
	Expr* fold() override;

	Expr* expr;
	Lisp<Value> values;
	Lisp<gcstring> packed;
	bool isterm;
	};

struct MultiOp : public Expr
	{
	explicit MultiOp(const Lisp<Expr*>& e) : exprs(e)
		{ }
	Fields fields() override;
	Lisp<Expr*> rename_exprs(const Fields& from, const Fields& to);
	Lisp<Expr*> replace_exprs(const Fields& from, const Lisp<Expr*>& to);
	Lisp<Expr*> fold_exprs();

	Lisp<Expr*> exprs;
	};

struct Or : public MultiOp
	{
	explicit Or(const Lisp<Expr*>& e) : MultiOp(e)
		{ }
	void out(Ostream& os) const override;
	Expr* rename(const Fields& from, const Fields& to) override;
	Expr* replace(const Fields& from, const Lisp<Expr*>& to) override;
	Expr* fold() override;
	Value eval(const Header& hdr, const Row& row) override;
	};

struct And : public MultiOp
	{
	explicit And(const Lisp<Expr*>& e) : MultiOp(e)
		{ }
	void out(Ostream& os) const override;
	Expr* rename(const Fields& from, const Fields& to) override;
	Expr* replace(const Fields& from, const Lisp<Expr*>& to) override;
	Expr* fold() override;
	Value eval(const Header& hdr, const Row& row) override;
	};

struct FunCall : public MultiOp
	{
	FunCall(Expr* e, const gcstring& f, const Lisp<Expr*>& args) : MultiOp(args), ob(e), fname(f)
		{ }
	void out(Ostream& os) const override;
	Fields fields() override;
	Expr* rename(const Fields& from, const Fields& to) override;
	Expr* replace(const Fields& from, const Lisp<Expr*>& to) override;
	Expr* fold() override
		{ return this; }
	Value eval(const Header& hdr, const Row& row) override;

	Expr* ob;
	gcstring fname;
	};
