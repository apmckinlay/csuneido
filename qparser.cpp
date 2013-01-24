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

#include <stdlib.h>
#include <ctype.h>
#include "std.h"
#include "suboolean.h"
#include "sunumber.h"
#include "sustring.h"
#include "query.h"
#include "qscanner.h"
#include "suobject.h"
#include "database.h"
#include "thedb.h"
#include "call.h"
#include "sudate.h"
#include "surecord.h"
#include "commalist.h"
#include "symbols.h"
#include "dbms.h"
#include "sesviews.h"
#include "assert.h"
#include "exceptimp.h"

struct Arg
	{
	gcstring param;
	Expr* expr;
	Arg(const gcstring& p, Expr* e) : param(p), expr(e)
		{ }
	bool operator!=(const gcstring& p) const
		{ return param != p; }
	};

struct IndexSpec
	{
	IndexSpec() : key(false), unique(false)
		{ }
	Lisp<gcstring> columns;
	bool key;
	bool unique;
	gcstring fktable;
	Lisp<gcstring> fkcols;
	Fkmode fkmode;
	};

struct TableSpec
	{
	Lisp<gcstring> columns;
	Lisp<IndexSpec> indexes;

	bool haskey();
	};

class ViewNest
	{
public:
	void push(const gcstring& view)
		{ views.push(view); }
	void pop(const gcstring& view)
		{
		assert(*views == view);
		++views;
		}
	bool inside(const gcstring& view)
		{ return member(views, view); }
private:
	Lisp<gcstring> views;
	};

class QueryParser
	{
public:
	explicit QueryParser(char*);
	friend bool database_admin(char* s);
	friend int database_request(int tran, char* s);
	friend Query* parse_query(char* s);
	friend Expr* parse_expr(char* s);
private:
	bool admin();
	int request(int tran);
	TableSpec table_spec();
	Lisp<gcstring> column_list();
	Fields join_by();
	SuRecord* record();
	Value constant();
	Value object();
	void member(SuObject* ob, char* gname = 0, short base = -1);
	ushort memname(char* gname, char* s);
	Value number();
	Query* query();
	Query* source();
	gcstring viewdef(const gcstring& table);
	Expr* expr();
	Expr* orop();
	Expr* andop();
	Expr* in();
	Expr* bitorop();
	Expr* bitxor();
	Expr* bitandop();
	Expr* isop();
	Expr* cmpop();
	Expr* shift();
	Expr* addop();
	Expr* mulop();
	Expr* unop();
	Expr* term();
	QueryScanner scanner;
	short token;
	void match(short t);
	void match();
	void back();
	void syntax_error();
	bool isfunc();

	int prevsi;
	ViewNest viewnest;
	};

bool is_admin(char* s)
	{
	QueryScanner scanner(s);
	scanner.next();
	switch (scanner.keyword)
		{
		case K_CREATE :
		case K_ALTER :
		case K_ENSURE :
		case K_DROP :
		case K_VIEW :
		case K_SVIEW :
		case K_RENAME :
			return true;
		default :
			return false;
		}
	}

bool is_request(char* s)
	{
	QueryScanner scanner(s);
	scanner.next();
	switch (scanner.keyword)
		{
		case K_INSERT :
		case K_UPDATE :
		case K_DELETE :
			return true;
		default :
			return false;
		}
	}

bool database_admin(char* s)
	{
	QueryParser parser(s);
	try
		{ return parser.admin(); }
	catch (const Except& e)
		{
		if (e.isBlockReturn())
			throw;
		else
			throw Except(e, "query: " + e.gcstr());
		}
	}

int database_request(int tran, char* s)
	{
	QueryParser parser(s);
	try
		{ return parser.request(tran); }
	catch (const Except& e)
		{
		if (e.isBlockReturn())
			throw;
		else
			throw Except(e, "query: " + e.gcstr());
		}
	}

Query* parse_query(char* s)
	{
	QueryParser parser(s);
	try
		{
		Query* q = parser.query();
		if (parser.token != Eof)
			parser.syntax_error();
		return q;
		}
	catch (const Except& e)
		{
		if (e.isBlockReturn())
			throw;
		else
			throw Except(e, "query: " + e.gcstr());
		}
	}

Expr* parse_expr(char* s)
	{
	QueryParser parser(s);
	return parser.expr();
	}

QueryParser::QueryParser(char* s) : scanner(s), prevsi(-1)
	{
	token = scanner.next();
	}

void QueryParser::match()
	{
	prevsi = scanner.si;
	token = scanner.next();
	}

void QueryParser::match(short t)
	{
	if (t != (t < KEYWORDS ? token : scanner.keyword))
		syntax_error();
	match();
	}

void QueryParser::back()
	{
	verify(prevsi != -1);
	scanner.si = prevsi;
	}

void QueryParser::syntax_error()
	{
	except("syntax error\n" <<
			gcstring(scanner.source, scanner.prev) << " @ " <<
			scanner.source + scanner.prev);
	}

bool TableSpec::haskey()
	{
	for (Lisp<IndexSpec> idx = indexes; ! nil(idx); ++idx)
		if (idx->key)
			return true;
	return false;
	}

bool QueryParser::admin()
	{
	switch (scanner.keyword)
		{
	case K_CREATE : {
		match();
		gcstring table = scanner.value;
		match(T_IDENTIFIER);
		TableSpec ts = table_spec();

		if (theDB()->istable(table))
			except("create: table already exists: " << table);

		try
			{
			theDB()->add_table(table);
			if (! ts.haskey())
				except("key required for: " << table);
			for (Lisp<gcstring> c = ts.columns; ! nil(c); ++c)
				theDB()->add_column(table, *c);
			for (Lisp<IndexSpec> i = ts.indexes; ! nil(i); ++i)
				theDB()->add_index(table, fields_to_commas(i->columns), i->key,
					i->fktable, fields_to_commas(i->fkcols), i->fkmode,
					i->unique);
			}
		catch (const Except& e)
			{
			if (theDB()->istable(table))
				theDB()->remove_table(table);
			if (e.isBlockReturn())
				throw;
			else
				throw Except(e, "create: " + e.gcstr());
			}
		return true;
		}
	case K_ENSURE :
		{
		match();
		gcstring table = scanner.value;
		match(T_IDENTIFIER);

		TableSpec ts = table_spec();
		bool table_created = false;
		try
			{
			if (! theDB()->istable(table))
				{
				bool haskey = false;
				for (Lisp<IndexSpec> i = ts.indexes; ! nil(i); ++i)
					haskey |= i->key;
				if (! haskey)
					except("key required for: " << table);
				theDB()->add_table(table);
				table_created = true;
				}

			Lisp<gcstring> cols = theDB()->get_columns(table);
			for (Lisp<gcstring> c = ts.columns; ! nil(c); ++c)
				{
				gcstring col(c->str());
				*col.buf() = tolower(*col.buf());
				if (! cols.member(col))
					theDB()->add_column(table, *c);
				}

			// TODO: handle changing index e.g. add foreign key
			Lisp<Fields> indexes = theDB()->get_indexes(table);
			for (Lisp<IndexSpec> i = ts.indexes; ! nil(i); ++i)
				if (! indexes.member(i->columns))
					theDB()->add_index(table, fields_to_commas(i->columns), i->key,
						i->fktable, fields_to_commas(i->fkcols), i->fkmode, i->unique);
			}
		catch (const Except& e)
			{
			if (table_created)
				theDB()->remove_table(table);
			if (e.isBlockReturn())
				throw;
			else
				throw Except(e, "ensure: " + e.gcstr());
			}

		return true;
		}
	case K_ALTER :
		{
		try
			{
			match();
			gcstring table = scanner.value;
			match(T_IDENTIFIER);
			if (scanner.keyword == K_RENAME)
				{
				Lisp<gcstring> from;
				Lisp<gcstring> to;
				do
					{
					match();
					from.push(scanner.value);
					match(T_IDENTIFIER);
					match(K_TO);
					to.push(scanner.value);
					match(T_IDENTIFIER);
					}
					while (token == ',');
				if (token != Eof)
					syntax_error();
				bool ok = true;
				for (from.reverse(), to.reverse(); ! nil(from); ++from, ++to)
					ok = ok && theDB()->rename_column(table, *from, *to);
				return ok;
				}
			int mode = scanner.keyword;
			if (scanner.keyword != K_CREATE &&
				scanner.keyword != K_DELETE && scanner.keyword != K_DROP)
				syntax_error();
			match();

			TableSpec ts = table_spec();
			if (! theDB()->istable(table))
				except("nonexistent table: " << table);

			for (Lisp<gcstring> c = ts.columns; ! nil(c); ++c)
				if (mode == K_CREATE)
					theDB()->add_column(table, *c);
				else // drop
					theDB()->remove_column(table, *c);
			for (Lisp<IndexSpec> i = ts.indexes; ! nil(i); ++i)
				if (mode == K_CREATE)
					theDB()->add_index(table, fields_to_commas(i->columns), i->key,
						i->fktable, fields_to_commas(i->fkcols), i->fkmode, i->unique);
				else // drop
					theDB()->remove_index(table, fields_to_commas(i->columns));
			}
		catch (const Except& e)
			{
			if (e.isBlockReturn())
				throw;
			else
				throw Except(e, "alter: " + e.gcstr());
			}
		return true;
		}
	case K_RENAME :
		{
		match();
		gcstring oldtblname = scanner.value;
		match(T_IDENTIFIER);
		match(K_TO);
		gcstring newtblname = scanner.value;
		match(T_IDENTIFIER);
		if (token != Eof)
			syntax_error();
		return theDB()->rename_table(oldtblname, newtblname);
		}
	case K_VIEW :
		{
		match();
		gcstring table = scanner.value;
		match(T_IDENTIFIER);
		gcstring def = scanner.rest();
		if (def == "")
			syntax_error();
		match(I_EQ);
		if (theDB()->istable(table) || viewdef(table) != "")
			except("view: '" << table << "' already exists");
		theDB()->add_view(table, def);
		return  true;
		}
	case K_SVIEW :
		{
		match();
		gcstring table = scanner.value;
		match(T_IDENTIFIER);
		gcstring def = scanner.rest();
		if (def == "")
			syntax_error();
		match(I_EQ);
		if (viewdef(table) != "")
			except("view: '" << table << "' already exists");
		set_session_view(table, def);
		return  true;
		}
	case K_DROP :
		{
		match();
		gcstring table = scanner.value;
		match(token == T_IDENTIFIER ? token : T_STRING);

		if (token != Eof)
			syntax_error();

		if (get_session_view(table) != "")
			remove_session_view(table);
		else if (theDB()->get_view(table) != "")
			theDB()->remove_view(table);
		else
			theDB()->remove_table(table);
		return true;
		}
	default :
		except("expecting: create, ensure, alter, rename, view, or drop");
		}
	}

#include "ostreamstr.h"

int QueryParser::request(int tran)
	{
	// TODO: don't do anything till you parse the whole request (& get Eof)
	switch (scanner.keyword)
		{
	case K_UPDATE :
		{
		match();
		Query* q = query();
		match(K_SET);
		Fields cols;
		Lisp<Expr*> exprs;
		while (token == T_IDENTIFIER)
			{
			cols.push(scanner.value);
			match(T_IDENTIFIER);
			match(I_EQ);
			exprs.push(expr());
			if (token == ',')
				match();
			}
		if (token != Eof)
			syntax_error();

		return update_request(tran, q, cols.reverse(), exprs.reverse());
		}
	case K_DELETE :
		{
		match();
		Query* q = query();
		if (token != Eof)
			syntax_error();
		return delete_request(tran, q);
		}
	case K_INSERT :
		{
		match();
		if (token == '{')
			{
			SuRecord* surec = record();
			match(K_INTO);
			Query* q = query();
			q->set_transaction(tran);

			if (token != Eof)
				syntax_error();

			return q->output(surec->to_record(q->header()))
				? 1 : 0;
			}
		else
			{
			Query* q = query();
			match(K_INTO);
			gcstring table = scanner.value;
			match(T_IDENTIFIER);

			if (token != Eof)
				syntax_error();

			// optimize the query
			q = query_setup(q);
			q->set_transaction(tran);

			// iterate thru query and add rows to destination
			Header hdr = q->header();
			Fields fields = theDB()->get_fields(table);
			Row row;
			int n = 0;
			for (; Query::Eof != (row = q->get(NEXT)); ++n)
				{
				Record r;
				for (Fields f = fields; ! nil(f); ++f)
					{
					if (*f == "-")
						r.addnil();
					else
						r.addraw(row.getraw(hdr, *f));
					}
				theDB()->add_record(tran, table, r);
				}
			q->close(q);
			return n;
			}
		break ;
		}
	default :
		except("expecting: insert, update, or delete");
		}
	return true;
	}

TableSpec QueryParser::table_spec()
	{
	TableSpec ts;

	// columns
	if (token == '(')
		{
		match('(');
		while (token != ')')
			{
			ts.columns.push(token == I_SUB ? "-" : scanner.value);
			if (token == I_SUB)
				match(I_SUB);
			else
				match(T_IDENTIFIER);
			if (token == ',')
				match(',');
			}
		match(')');
		ts.columns.reverse();
		}
	// triggers & indexes
	while (token != Eof)
		{
		if (scanner.keyword == K_KEY || scanner.keyword == K_INDEX)
			{
			// INDEX ( fields )  KEY ( fields )
			IndexSpec idx;
			idx.key = (scanner.keyword == K_KEY);
			match();
			if ((idx.unique = (scanner.keyword == K_UNIQUE)))
				match();
			idx.columns = column_list();
			idx.fkmode = BLOCK;
			if (scanner.keyword == K_IN)
				{
				match();
				idx.fktable = scanner.value;
				match(T_IDENTIFIER);
				idx.fkcols = (token == '(') ? column_list() : idx.columns.copy();
				if (scanner.keyword == K_CASCADE)
					{
					match();
					idx.fkmode = CASCADE;
					if (scanner.keyword == K_UPDATE)
						{
						match();
						idx.fkmode = CASCADE_UPDATES;
						}
					}
				}
			ts.indexes.push(idx);
			}
		else
			syntax_error();
		}
	ts.indexes.reverse(); // cosmetic
	return ts;
	}

Lisp<gcstring> QueryParser::column_list()
	{
	Lisp<gcstring> columns;
	match('(');
	while (token != ')')
		{
		columns.push(scanner.value);
		match(T_IDENTIFIER);
		if (token == ',')
			match(',');
		}
	match(')');
	return columns.reverse();
	}

SuRecord* QueryParser::record()
	{
	SuRecord* surec = new SuRecord();
	match('{');
	while (token != '}')
		{
		gcstring col = scanner.value;
		match();
		match(':');
		surec->putdata(col, constant());
		if (token == ',')
			match(',');
		}
	match('}');
	return surec;
	}

Value QueryParser::constant()
	{
	Value x;
	switch (token)
		{
	case I_SUB :
		match(I_SUB);
		x = -number();
		break ;
	case T_NUMBER :
		x = number();
		break ;
	case T_STRING :
		if (scanner.len == 0)
			x = SuString::empty_string;
		else
			x = new SuString(scanner.value, scanner.len);
		match();
		break ;
	case '#' :
		match();
		// fall thru
	case '[' :
		if (token == '(' || token == '{' || token == '[')
			x = object();
		else if (token == T_NUMBER)
			{
			if (! (x = SuDate::literal(scanner.value)))
				syntax_error();
			match(T_NUMBER);
			}
		else if (token == T_IDENTIFIER || token == T_STRING ||
			token == T_AND || token == T_OR || token == I_NOT ||
			token == I_IS || token == I_ISNT)
			{
			x = symbol(scanner.value);
			match();
			return x;
			}
		else
			syntax_error();
		break ;
	case T_IDENTIFIER :
		if (scanner.keyword == K_TRUE)
			{ x = SuBoolean::t; match(); break ; }
		else if (scanner.keyword == K_FALSE)
			{ x = SuBoolean::f; match(); break ; }
		// else fall thru to syntax_error
	default :
		syntax_error();
		}
 	return x;
	}

Value QueryParser::number()
	{
	Value x = SuNumber::literal(scanner.value);
	match(T_NUMBER);
	return x;
	}

Value QueryParser::object()
	{
	SuObject* ob = 0;
	char end = 0;
	if (token == '(')
		{
		ob = new SuObject();
		end = ')';
		match();
		}
	else if (token == '{' || token == '[')
		{
		ob = new SuRecord();
		end = token == '{' ? '}' : ']';
		match();
		}
	else
		syntax_error();
	while (token != end)
		{
		member(ob);
		if (token == ',' || token == ';')
			match();
		}
	match(end);
	ob->set_readonly();
	return ob;
	}

// TODO: remove gname - not used
void QueryParser::member(SuObject* ob, char* gname, short base)
	{
	Value mv;
	int mi = -1;
	bool minus = false;
	if (token == I_SUB)
		{
		minus = true;
		match();
		if (token != T_NUMBER)
			syntax_error();
		}
	bool default_allowed = true;
	char peek = *scanner.peek();
	if (peek == ':' || (base > 0 && peek == '('))
		{
		if (token == T_IDENTIFIER || token == T_STRING)
			{
			mv = symbol(mi = symnum(scanner.value));
			match();
			}
		else if (token == T_NUMBER)
			{
			mv = number();
			if (minus)
				{
				mv = -mv;
				minus = false;
				}
			}
		else
			syntax_error();
		if (token == ':')
			match();
		}
	else
		default_allowed = false;

	Value x;
	if (token != ',' && token != ')' && token != '}')
		{
		x = constant();
		if (minus)
			x = -x;
		}
	else if (default_allowed)
		x = SuBoolean::t; // default value
	else
		syntax_error();

	if (mv)
		{
		if (ob->get(mv))
			except("duplicate member name (" << mv << ")");
		ob->put(mv, x);
		}
	else
		ob->add(x);
	}

Query* QueryParser::query()
	{
	Query* q = source();
	while (token != Eof && token != ')')
		{
		switch (scanner.keyword)
			{
		case K_WHERE :
			match();
			q = Query::make_select(q, expr());
			break ;
		case K_SORT :
			{
			match();
			bool reverse = false;
			if (scanner.keyword == K_REVERSE)
				{
				reverse = true;
				match();
				}
			Fields segs;
			for (;;)
				{
				segs.push(scanner.value);
				match(T_IDENTIFIER);
				if (token != ',')
					break ;
				match(',');
				}
			q = Query::make_sort(q, reverse, segs.reverse());
			if (token != Eof)
				syntax_error(); // sort must be last operation
			break ;
			}
		case K_PROJECT:
		case K_REMOVE :
			{
			bool allbut = scanner.keyword == K_REMOVE;
			Fields flds;
			do
				{
				match();
				flds.push(scanner.value);
				match(T_IDENTIFIER);
				}
				while (token == ',');
			q = Query::make_project(q, flds.reverse(), allbut);
			break ;
			}
		case K_SUMMARIZE:
			{
			Fields sumproj;
			Fields sumfuncs;
			Fields sumon;
			Fields sumcols;
			gcstring fld;
			do
				{
				fld = "";
				match();
				if (token == T_IDENTIFIER && ! isfunc())
					{
					fld = scanner.value;
					match(T_IDENTIFIER);
					if (token == I_EQ)
						match();
					else
						{ sumproj.push(fld); continue ; }
					}
				switch (scanner.keyword)
					{
				case K_TOTAL :
				case K_AVERAGE :
				case K_MIN :
				case K_MAX :
				case K_LIST :
					{
					gcstring func = scanner.value;
					sumfuncs.push(func);
					match();
					sumon.push(scanner.value);
					if (fld == "")
						fld = func + "_" + scanner.value;
					sumcols.push(fld);
					match(T_IDENTIFIER);
					break ;
					}
				case K_COUNT :
					sumfuncs.push("count");
					match();
					sumon.push("");
					if (fld == "")
						fld = "count";
					sumcols.push(fld);
					break ;
				default :
					syntax_error();
					}
			    }
			    while (token == ',');
			if (nil(sumcols))
				syntax_error();
			q = Query::make_summarize(q, sumproj.reverse(), sumcols.reverse(), sumfuncs.reverse(), sumon.reverse());
			break ;
			}
		case K_JOIN :
			{
			match();
			Fields by = join_by();
			q = Query::make_join(q, source(), by);
			break ;
			}
		case K_LEFTJOIN :
			{
			match();
			Fields by = join_by();
			q = Query::make_leftjoin(q, source(), by);
			break ;
			}
		case K_TIMES :
			match();
			q = Query::make_product(q, source());
			break ;
		case K_UNION :
			match();
			q = Query::make_union(q, source());
			break ;
		case K_INTERSECT :
			match();
			q = Query::make_intersect(q, source());
			break ;
		case K_DIFFERENCE :
			match();
			q = Query::make_difference(q, source());
			break ;
		case K_RENAME :
			{
			Fields from;
			Fields to;
			do
				{
				match();
				from.push(scanner.value);
				match(T_IDENTIFIER);
				match(K_TO);
				to.push(scanner.value);
				match(T_IDENTIFIER);
				}
				while (token == ',');
			q = Query::make_rename(q, from.reverse(), to.reverse());
			break ;
			}
		case K_EXTEND :
			{
			Fields eflds;
			Lisp<Expr*> exprs;
			do
				{
				match(); // EXTEND or ','
				gcstring field;
				field = scanner.value;
				match(T_IDENTIFIER);
				eflds.push(field);
				if (token == I_EQ)
					{
					match(I_EQ);
					exprs.push(expr());
					}
				else
					exprs.push(NULL);
				}
				while (token == ',');
			q = Query::make_extend(q, eflds.reverse(), exprs.reverse());
			break ;
			}
		default :
			return q;
			}
		}
	return q;
	}

Fields QueryParser::join_by()
	{
	Fields by;
	if (scanner.keyword == K_BY)
		{
		match();
		by = column_list();
		if (nil(by))
			syntax_error();
		}
	return by;
	}

bool QueryParser::isfunc()
	{
	return scanner.keyword == K_COUNT ||
		scanner.keyword == K_MIN ||
		scanner.keyword == K_MAX ||
		scanner.keyword == K_TOTAL ||
		scanner.keyword == K_AVERAGE ||
		scanner.keyword == K_LIST;
	}

Query* QueryParser::source()
	{
	Query* q = 0;
	if (token == T_IDENTIFIER)
		{
		gcstring table = scanner.value;
		match(T_IDENTIFIER);
		if (table == "history" && token == '(')
			{
			match('(');
			table = scanner.value;
			match(T_IDENTIFIER);
			match(')');
			return Query::make_history(table);
			}
		// check whether it's a view definition
		gcstring def = viewdef(table);
		if (def == "" || viewnest.inside(table))
			return Query::make_table(table.str());
		else
			{
			back();
			scanner.insert(def);
			token = scanner.next();
			viewnest.push(table);
			match('(');
			q = query();
			match(')');
			viewnest.pop(table);
			}
		}
	else if (token == '(')
		{
		match('(');
		q = query();
		match(')');
		}
	else
		syntax_error();
	return q;
	}

gcstring QueryParser::viewdef(const gcstring& table)
	{
	gcstring def = get_session_view(table);
	if (def == "")
		def = theDB()->get_view(table);
	return def == "" ? def : "(" + def + ")";
	}

Expr* QueryParser::expr()
	{
	Expr* e = orop();
	if (token == '?')
		{
		match('?');
		Expr* t = expr();
		match(':');
		Expr* f = expr();
		e = Query::make_triop(e, t, f);
		}
	return e;
	}

Expr* QueryParser::orop()
	{
	Expr* e = andop();
	if (token == T_OR)
		{
		Lisp<Expr*> exprs(e);
		do
			{
			match();
			exprs.push(andop());
			}
			while (token == T_OR);
/* TODO: convert to IN if possible
		Lisp<Constant> values;
		Identifier* id = 0;
		Lisp<Expr*> x(exprs);
		for (; ! nil(x); ++x)
			{
			BinOp* b = dynamic_cast<BinOp*>(*x);
			if (! b) break ;
			Identifier* left = dynamic_cast<Identifier*>(b->left);
			if (! left) break ;
			if (! id)
				id = left;
			Constant* right = dynamic_cast<Constant*>(b->right);
			if (! right || left->ident != id->ident || b->op != I_IS)
				break ;
			values.push(*right);
			}
		if (nil(x))
			e = Query::make_in(id, values);
		else
*/
			e = Query::make_or(exprs.reverse());
		}
	return e;
	}

Expr* QueryParser::andop()
	{
	Expr* e = in();
	if (token == T_AND)
		{
		Lisp<Expr*> exprs(e);
		do
			{
			match();
			exprs.push(in());
			}
			while (token == T_AND);
		e = Query::make_and(exprs.reverse());
		}
	return e;
	}

Expr* QueryParser::in()
	{
	// TODO: should check for duplicate values
	Expr* e = bitorop();
	if (scanner.keyword == K_IN)
		{
		match();
		match('(');
		Lisp<Value> values;
		while (token != ')')
			{
			values.push(constant());
			if (token == ',')
				match();
			}
		match(')');
		e = Query::make_in(e, values.reverse());
		}
	return e;
	}

Expr* QueryParser::bitorop()
	{
	Expr* e = bitxor();
	while (token == I_BITOR)
		{
		match();
		e = Query::make_binop(I_BITOR, e, bitxor());
		}
	return e;
	}

Expr* QueryParser::bitxor()
	{
	Expr* e = bitandop();
	while (token == I_BITXOR)
		{
		match();
		e = Query::make_binop(I_BITXOR, e, bitandop());
		}
	return e;
	}

Expr* QueryParser::bitandop()
	{
	Expr* e = isop();
	while (token == I_BITAND)
		{
		match();
		e = Query::make_binop(I_BITAND, e, isop());
		}
	return e;
	}

Expr* QueryParser::isop()
	{
	Expr* e = cmpop();
	if (token == I_EQ)
		token = I_IS;
	while (I_IS <= token && token <= I_MATCHNOT)
		{
		short t = token;
		match(t);
		e = Query::make_binop(t, e, cmpop());
		}
	return e;
	}

Expr* QueryParser::cmpop()
	{
	Expr* e = shift();
	while (token == I_LT || token == I_LTE || token == I_GT || token == I_GTE)
		{
		short t = token;
		match(t);
		e = Query::make_binop(t, e, shift());
		}
	return e;
	}

Expr* QueryParser::shift()
	{
	Expr* e = addop();
	while (token == I_LSHIFT || token == I_RSHIFT)
		{
		short t = token;
		match(t);
		e = Query::make_binop(t, e, addop());
		}
	return e;
	}

Expr* QueryParser::addop()
	{
	Expr* e = mulop();
	while (token == I_ADD || token == I_SUB || token == I_CAT)
		{
		short t = token;
		match(t);
		e = Query::make_binop(t, e, mulop());
		}
	return e;
	}

Expr* QueryParser::mulop()
	{
	Expr* e = unop();
	while (token == I_MUL || token == I_DIV || token == I_MOD)
		{
		short t = token;
		match(t);
		e = Query::make_binop(t, e, unop());
		}
	return e;
	}

Expr* QueryParser::unop()
	{
	if (token == I_NOT || token == I_ADD || token == I_SUB || token == I_BITNOT)
		{
		short t = token;
		match(t);
		return Query::make_unop(t, unop());
		}
	else
		return term();
	}

Expr* QueryParser::term()
	{
	Expr* t = 0;
	switch (token)
		{
	case T_NUMBER :
	case T_STRING :
	case '#' :
	case '[' :
		t = Query::make_constant(constant());
		break ;
	case T_IDENTIFIER :
		{
		if (scanner.keyword == K_TRUE || scanner.keyword == K_FALSE)
			t = Query::make_constant(constant());
		else
			{
			Expr* ob = NULL;
			gcstring id(scanner.value);
			match(T_IDENTIFIER);
			if (token != '(' && token != '.')
				t = Query::make_identifier(id);
			else
				{
				if (token == '.') {
					match('.');
					ob = Query::make_identifier(id);
					id = scanner.value;
					match(T_IDENTIFIER);
				}
				match('(');
				Lisp<Expr*> exprs;
				while (token != ')')
					{
					exprs.push(expr());
					if (token != ')')
						match(',');
					}
				match(')');
				t = Query::make_call(ob, id, exprs.reverse());
				}
			}
		break ;
		}
	case '(' :
		match('(');
		t = expr();
		match(')');
		break ;
	default :
		syntax_error();
		}
	return t;
	}

#include "testing.h"
#include "ostreamstr.h"

struct Qptest
	{
	char* query;
	char* result;
	};

static Qptest qptests[] =
	{
		{ "inven", "inven" },
		{ "inven sort item", "inven SORT (item)" },
		{ "inven sort qty", "inven SORT (qty)" },

		{ "trans union hist", "(trans) UNION-LOOKUP (hist) " },
		{ "trans union hist sort item", "(trans) UNION-LOOKUP (hist)  SORT (item)" },
		{ "trans union hist sort date", "(trans) UNION-LOOKUP (hist)  SORT (date)" },

		{ "trans join inven", "(trans) JOIN n:1 on (item) (inven)" },
		{ "trans join inven sort item", "(trans) JOIN n:1 on (item) (inven) SORT (item)" },
		{ "trans join inven sort date", "(trans) JOIN n:1 on (item) (inven) SORT (date)" },

		{ "trans join customer", "(trans) JOIN n:1 on (id) (customer)" },
		{ "trans join customer sort id", "(trans) JOIN n:1 on (id) (customer) SORT (id)" },
		{ "trans join customer sort name", "(trans) JOIN n:1 on (id) (customer) SORT (name)" },
		{ "trans join customer join inven", "((trans) JOIN n:1 on (id) (customer)) JOIN n:1 on (item) (inven)" },

		{ "trans join inven join customer", "((trans) JOIN n:1 on (item) (inven)) JOIN n:1 on (id) (customer)" },

		{ "inven join trans join customer", "((inven) JOIN 1:n on (item) (trans)) JOIN n:1 on (id) (customer)" },
		{ "customer join trans join inven", "((customer) JOIN 1:n on (id) (trans)) JOIN n:1 on (item) (inven)" },

		{ "trans union hist join inven", "((trans) UNION-LOOKUP (hist) ) JOIN n:1 on (item) (inven)" },
		{ "trans union hist join inven sort item", "((trans) UNION-LOOKUP (hist) ) JOIN n:1 on (item) (inven) SORT (item)" },
		{ "trans union hist join inven sort date", "((trans) UNION-LOOKUP (hist) ) JOIN n:1 on (item) (inven) SORT (date)" },

		{ "trans union hist join customer", "((trans) UNION-LOOKUP (hist) ) JOIN n:1 on (id) (customer)" },
		{ "trans union hist join customer sort item", "((trans) UNION-LOOKUP (hist) ) JOIN n:1 on (id) (customer) SORT (item)"  },
		{ "trans union hist join customer sort id", "((trans) UNION-LOOKUP (hist) ) JOIN n:1 on (id) (customer) SORT (id)" },
		{ "trans union hist join customer join inven", "(((trans) UNION-LOOKUP (hist) ) JOIN n:1 on (id) (customer)) JOIN n:1 on (item) (inven)" },
		{ "trans union hist join customer join inven sort item", "(((trans) UNION-LOOKUP (hist) ) JOIN n:1 on (id) (customer)) JOIN n:1 on (item) (inven) SORT (item)" },
		{ "trans union hist join customer join inven sort date", "(((trans) UNION-LOOKUP (hist) ) JOIN n:1 on (id) (customer)) JOIN n:1 on (item) (inven) SORT (date)" },

		{ "inven where item", "inven WHERE item" },
		{ "trans union hist where item", "(trans) UNION-LOOKUP (hist)  WHERE item" },

		{ "trans join customer where name", "(trans) JOIN n:1 on (id) (customer) WHERE name" },
		{ "trans join customer where date", "(trans) JOIN n:1 on (id) (customer) WHERE date" },
		{ "trans join customer where id", "(trans) JOIN n:1 on (id) (customer) WHERE id" },

		{ "hist union trans where date = 1", "(hist) UNION-LOOKUP (trans)  WHERE (date == 1)" },
		{ "hist join inven where date = 1", "(hist) JOIN n:1 on (item) (inven) WHERE (date == 1)" },
		{ "hist join inven where qty = 1", "(hist) JOIN n:1 on (item) (inven) WHERE (qty == 1)" },
		{ "hist join inven where date < qty", "(hist) JOIN n:1 on (item) (inven) WHERE (date < qty)" },
		{ "hist join inven where date = 1 and qty = 2 and date < qty", "(hist) JOIN n:1 on (item) (inven) WHERE ((date == 1) and (qty == 2) and (date < qty))" },

		{ "inven where item = 1 where qty = 2", "inven WHERE (item == 1) WHERE (qty == 2)" },
		{ "inven where item = 1 join trans where qty = 2", "(inven WHERE (item == 1)) JOIN 1:n on (item) (trans) WHERE (qty == 2)" },

		{ "hist project id,date", "hist PROJECT (id,date)" },
		{ "hist project date", "hist PROJECT (date)" },
		{ "hist project id", "hist PROJECT (id)" },
		{ "hist project date,cost sort cost", "hist PROJECT (date,cost) SORT (cost)" },

		{ "customer join hist sort date", "(customer) JOIN 1:n on (id) (hist) SORT (date)" },
		{ "customer join hist sort item", "(customer) JOIN 1:n on (id) (hist) SORT (item)" },
		{ "customer join hist sort name", "(customer) JOIN 1:n on (id) (hist) SORT (name)" },

		{ "customer times inven", "(customer) TIMES (inven)" },
		{ "customer times inven sort item", "(customer) TIMES (inven) SORT (item)" },
		{ "inven times customer", "(inven) TIMES (customer)" },
		{ "inven times customer sort id", "(inven) TIMES (customer) SORT (id)" },

		{ "customer join alias", "(customer) JOIN 1:1 on (id) (alias)" },
		{ "customer join hist", "(customer) JOIN 1:n on (id) (hist)" },
		{ "hist join customer", "(hist) JOIN n:1 on (id) (customer)" },
		{ "customer join supplier", "(customer) JOIN n:n on (name,city) (supplier)" },
		{ "customer join by (id) hist", "(customer) JOIN 1:n on (id) (hist)" },
		{ "customer join by (name, city) supplier", "(customer) JOIN n:n on (name,city) (supplier)" },

		{ "hist intersect trans", "(hist) INTERSECT (trans) " },
		{ "hist intersect trans sort item", "(hist) INTERSECT (trans)  SORT (item)" },
		{ "trans intersect hist", "(trans) INTERSECT (hist) " },
		{ "trans intersect hist sort date", "(trans) INTERSECT (hist)  SORT (date)" },
		{ "trans union (hist where date = 1)", "(trans) UNION-LOOKUP (hist WHERE (date == 1)) " }
	};

class test_qparser : public  Tests
	{
	void adm(char* s)
		{
		except_if(! database_admin(s), "FAILED: " << s);
		}
	int tran;
	void req(char* s)
		{
		except_if(! database_request(tran, s), "FAILED: " << s);
		}
	TEST(0, main)
		{
		tran = theDB()->transaction(READWRITE);
		//create hist file
		database_admin("drop hist");
		adm("create hist (date,item,id,cost) index(date) key(date,item,id)");
		req("insert{date: 970101, item: \"disk\", id: \"a\", cost: 100} into hist");
		req("insert{date: 970101, item: \"disk\", id: \"e\", cost: 200} into hist");
		req("insert{date: 970102, item: \"mouse\", id: \"c\", cost: 200} into hist");
		req("insert{date: 970103, item: \"pencil\", id: \"e\", cost: 300} into hist");

		//create customer file
		database_admin("drop customer");
		adm("create customer (id:string,name:string,city:string) key(id)");
		req("insert{id: \"a\", name: \"axon\", city: \"saskatoon\"} into customer");
		req("insert{id: \"c\", name: \"calac\", city: \"calgary\"} into customer");
		req("insert{id: \"e\", name: \"emerald\", city: \"vancouver\"} into customer");
		req("insert{id: \"i\", name: \"intercon\", city: \"saskatoon\"} into customer");

		//create trans file
		database_admin("drop trans");
		adm("create trans (date:int,item:string,id:string,cost:number) index(item) key(date,item,id)");
		req("insert{item: \"mouse\", id: \"e\", cost: 200, date: 960204} into trans");
		req("insert{item: \"disk\", id: \"a\", cost: 100, date: 970101} into trans");
		req("insert{item: \"mouse\", id: \"c\", cost: 300, date: 970101} into trans");
		req("insert{item: \"eraser\", id: \"c\", cost: 150, date: 970201} into trans");

		//create supplier file
		database_admin("drop supplier");
		adm("create supplier (supplier:string, name:string, city:string) index(city) key(supplier)");
		req("insert{supplier: \"mec\", name: \"mtnequipcoop\", city: \"calgary\"} into supplier");
		req("insert{supplier: \"hobo\", name: \"hoboshop\", city: \"saskatoon\"} into supplier");
		req("insert{supplier: \"ebs\", name: \"ebssait&sport\", city: \"saskatoon\"} into supplier");
		req("insert{supplier: \"taiga\", name: \"taigaworks\", city: \"vancouver\"} into supplier");

		//create inven file
		database_admin("drop inven");
		adm("create inven (item:string, qty:number) key(item)");
		req("insert{item: \"disk\", qty: 5} into inven");
		req("insert{item: \"mouse\", qty:2} into inven");
		req("insert{item: \"pencil\", qty: 7} into inven");

		//create alias file
		database_admin("drop alias");
		adm("create alias(id, name2) key(id)");
		req("insert{id: \"a\", name2: \"abc\"} into alias");
		req("insert{id: \"c\", name2: \"trical\"} into alias");

		verify(theDB()->commit(tran));

		for (int i = 0; i < sizeof qptests / sizeof (Qptest); ++i)
			testone(i, qptests[i].query, qptests[i].result);

		adm("drop hist");
		adm("drop customer");
		adm("drop trans");
		adm("drop supplier");
		adm("drop inven");
		adm("drop alias");
		}
	void testone(int i, char* query, char* result)
		{
		Query* q = parse_query(query);
		OstreamStr out;
		out << *q;
		except_if(0 != strcmp(result, out.str()),
			i << ": " << query << "\n\t=> " << result << "\n\t!= '" << out.str() << "'");
		}
	TEST(2, joinby)
		{
		xassert(parse_query("tables join by (x) indexes"));
		}
	};
