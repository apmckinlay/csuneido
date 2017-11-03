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

#include "sudb.h"
#include "builtinclass.h"
#include "suclass.h"
#include "surecord.h"
#include "sustring.h"
#include "interp.h"
#include "dbms.h"
#include "except.h"
#include "exceptimp.h"
#include "symbols.h"
#include "globals.h"
#include "query.h" // for is_request
#include "ostreamstr.h"
#include "func.h"
#include "builtinargs.h"
#include "trace.h"

class DatabaseClass : public SuValue
	{
public:
	static auto methods()
		{
		return gsl::span<Method<DatabaseClass>>(); // none
		}

	static Value Transactions(BuiltinArgs&);
	static Value CurrentSize(BuiltinArgs&);
	static Value Connections(BuiltinArgs&);
	static Value TempDest(BuiltinArgs&);
	static Value Cursors(BuiltinArgs&);
	static Value SessionId(BuiltinArgs&);
	static Value Final(BuiltinArgs&);
	static Value Kill(BuiltinArgs&);
	static Value Check(BuiltinArgs&);
	static Value Dump(BuiltinArgs&);
	static Value Load(BuiltinArgs&);
	static Value Nonce(BuiltinArgs&);
	static Value Token(BuiltinArgs&);
	static Value Auth(BuiltinArgs&);
	};

Value su_Database()
	{
	static BuiltinClass<DatabaseClass> clazz("(request)");
	return &clazz;
	}

template<>
auto BuiltinClass<DatabaseClass>::static_methods()
	{
	static StaticMethod methods[]
		{
		{ "Transactions", &DatabaseClass::Transactions },
		{ "CurrentSize", &DatabaseClass::CurrentSize },
		{ "Connections", &DatabaseClass::Connections },
		{ "TempDest", &DatabaseClass::TempDest },
		{ "Cursors", &DatabaseClass::Cursors },
		{ "SessionId", &DatabaseClass::SessionId },
		{ "Final", &DatabaseClass::Final },
		{ "Kill", &DatabaseClass::Kill },
		{ "Check", &DatabaseClass::Check },
		{ "Dump", &DatabaseClass::Dump },
		{ "Load", &DatabaseClass::Load },
		{ "Nonce", &DatabaseClass::Nonce },
		{ "Token", &DatabaseClass::Token },
		{ "Auth", &DatabaseClass::Auth },
		};
	return gsl::make_span(methods);
	}

template<>
Value BuiltinClass<DatabaseClass>::callclass(BuiltinArgs& args)
	{
	args.usage("Database(request)");
	auto req = args.getstr("request");
	args.end();
	dbms()->admin(req);
	return Value();
	}

template<>
Value BuiltinClass<DatabaseClass>::instantiate(BuiltinArgs&)
	{
	except("can't create instance of Database");
	}

Value DatabaseClass::Transactions(BuiltinArgs& args)
	{
	SuObject* ob = new SuObject();
	for (Lisp<int> trans = dbms()->tranlist(); !nil(trans); ++trans)
		ob->add(*trans);
	return ob;
	}

Value DatabaseClass::CurrentSize(BuiltinArgs& args)
	{
	args.usage("Database.CurrentSize()").end();
	return SuNumber::from_int64(dbms()->size());
	}

Value DatabaseClass::Connections(BuiltinArgs& args)
	{
	args.usage("Database.Connections()").end();
	return dbms()->connections();
	}

Value DatabaseClass::TempDest(BuiltinArgs& args)
	{
	args.usage("Database.TempDest()").end();
	return dbms()->tempdest();
	}

Value DatabaseClass::Cursors(BuiltinArgs& args)
	{
	args.usage("Database.Cursors()").end();
	return dbms()->cursors();
	}

Value DatabaseClass::SessionId(BuiltinArgs& args)
	{
	args.usage("Database.SessionId(newid = '')");
	auto s = args.getstr("string", "");
	args.end();
	return dbms()->sessionid(s);
	}

Value DatabaseClass::Final(BuiltinArgs& args)
	{
	args.usage("Database.Final()").end();
	return dbms()->final();
	}

Value DatabaseClass::Kill(BuiltinArgs& args)
	{
	args.usage("Database.Kill(session_id)");
	auto s = args.getstr("string");
	args.end();
	return dbms()->kill(s);
	}

Value DatabaseClass::Check(BuiltinArgs& args)
	{
	args.usage("Database.Check()").end();
	return dbms()->check();
	}

Value DatabaseClass::Dump(BuiltinArgs& args)
	{
	args.usage("Database.Dump(table = '')");
	auto what = args.getstr("string", "");
	args.end();
	Value result = dbms()->dump(what);
	if (result != SuEmptyString)
		except("Database.Dump failed: " << result);
	return Value();
	}

Value DatabaseClass::Load(BuiltinArgs& args)
	{
	args.usage("Database.Load(table)");
	auto table = args.getstr("table");
	args.end();
	int result = dbms()->load(table);
	if (result < 0)
		except("Database.Load failed: " << result);
	return result;
	}

Value DatabaseClass::Nonce(BuiltinArgs& args)
	{
	args.usage("Database.Nonce()").end();
	return new SuString(dbms()->nonce());
	}

Value DatabaseClass::Token(BuiltinArgs& args)
	{
	args.usage("Database.Token()").end();
	return new SuString(dbms()->token());
	}

Value DatabaseClass::Auth(BuiltinArgs& args)
	{
	args.usage("Database.Auth(string)");
	auto s = args.getgcstr("string");
	return dbms()->auth(s) ? SuTrue : SuFalse;
	}

// Query1/First/Last ------------------------------------------------

static const char* query_args(const char* query, BuiltinArgs& args)
	{
	args.end(); // shouldn't be any more un-named args
	if (! args.hasNamed())
		return query;
	OstreamStr os;
	os << query;
	static int BLOCK = symnum("block");
	while (Value value = args.getNext())
		{
		ushort name = args.curName();
		if (name == BLOCK && dynamic_cast<Func*>(value.ptr()))
			continue ;
		os << " where " << symstr(name) << " = " << value;
		}
	return os.str();
	}

static const char* traceTran(SuTransaction* tran)
	{
	if (! tran)
		return "";
	OstreamStr os;
	os << 'T' << tran->tran << ' ';
	return os.str();
	}

#define QUERYONE_PARAMS "(query [, field: value ...])"

static Value queryone(const char* which, Dir dir, bool one, 
	SuTransaction* tran, BuiltinArgs& args)
	{
	if (tran)
		tran->checkNotEnded("query");
	args.usage("", which, QUERYONE_PARAMS);
	auto query = args.getstr("query");
	query = query_args(query, args);
	TRACE(QUERY, traceTran(tran) <<
		(one ? "ONE" : dir == NEXT ? "FIRST" : "LAST") << ' ' << query);
	Header hdr;
	Row row = dbms()->get(dir, query, one, hdr, tran ? tran->tran : NO_TRAN);
	return row == Row::Eof ? SuFalse : new SuRecord(row, hdr, tran);
	}

class QueryOne : public SuValue
	{
public:
	NAMED
	QueryOne(const char* w, Dir d, bool o) : which(w), dir(d), one(o)
		{
		named.num = globals("Query1");
		}
	Value call(Value self, Value member,
		short nargs, short nargnames, ushort* argnames, int each) override
		{
		static Value Params("Params");

		BuiltinArgs args(nargs, nargnames, argnames, each);
		if (member == CALL)
			return queryone(which, dir, one, NULL, args);
		else if (member == Params)
			{
			args.usage("", which, ".Params()").end();
			return new SuString(QUERYONE_PARAMS);
			}
		else
			method_not_found(which, member);
		}
	virtual const char* type() const override
		{ return "Builtin"; }
	void out(Ostream& os) const override
		{ os << which << " /* function */"; }
private:
	const char* which;
	Dir dir;
	bool one;
	};
Value su_Query1()
	{ return new QueryOne("Query1", NEXT, true); }
Value su_QueryFirst()
	{ return new QueryOne("QueryFirst", NEXT, false); }
Value su_QueryLast()
	{ return new QueryOne("QueryLast", PREV, false); }

Value su_transactions()
	{
	SuObject* ob = new SuObject();
	for (Lisp<int> trans = dbms()->tranlist(); ! nil(trans); ++trans)
		ob->add(*trans);
	return ob;
	}

// SuTransaction ------------------------------------------------------

Value TransactionClass::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	if (member == CALL || member == INSTANTIATE)
		{
		static ushort read = ::symnum("read");
		static ushort update = ::symnum("update");
		if (nargs < 1 || nargs > 2 || nargnames < 1 ||
			(argnames[0] != read && argnames[0] != update))
			except("usage: Transaction(read: [, block ]) or Transaction(update: [, block ])");
		SuTransaction* t = new SuTransaction(
			argnames[0] == update && ARG(0).toBool()
				? SuTransaction::READWRITE : SuTransaction::READONLY);
		if (nargs == 1)
			return t;
		// else do block with transaction
		Value block = ARG(1);
		try
			{
			KEEPSP
			PUSH(t);
			Value result = block.call(block, CALL, 1);
			if (! t->isdone())
				if (! t->commit())
					except("Transaction: block commit failed: " << t->get_conflict());
			return result;
			}
		catch (const Except& e)
			{
			if (! t->isdone())
				{
				// what about block:break/continue ?
				if (! e.isBlockReturn())
					t->rollback();
				else if (! t->commit())
					except("Transaction: block commit failed: " << t->get_conflict());
				}
			throw ;
			}
		}
	else
		return RootClass::notfound(self, member, nargs, nargnames, argnames, each);
	}

SuTransaction::SuTransaction(TranType type)
	{
	tran = dbms()->transaction((Dbms::TranType) type);
	}

SuTransaction::SuTransaction(int t) : tran(t), done(false), conflict("")
	{ verify(t > 0); }

Value SuTransaction::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value Query("Query");
	static Value Complete("Complete");
	static Value Rollback("Rollback");
	static Value UpdateQ("Update?");
	static Value EndedQ("Ended?");
	static Value QueryFirst("QueryFirst");
	static Value QueryLast("QueryLast");
	static Value Query1("Query1");
	static Value Conflict("Conflict");
	static Value ReadCount("ReadCount");
	static Value WriteCount("WriteCount");
	static Value Data("Data");

	BuiltinArgs args(nargs, nargnames, argnames, each);
	if (member == Query)
		{
		args.usage("transaction.Query(value [, block][, field: value...])");
		auto qstr = args.getstr("query");
		Value block = args.getValue("block", Value());
		if (block && ! dynamic_cast<Func*>(block.ptr()))
			args.exceptUsage();
		qstr = query_args(qstr, args);
		TRACE(QUERY, 'T' << tran << ' ' << qstr);
		Value q = query(qstr);
		if (! block)
			return q;
		if (! dynamic_cast<SuQuery*>(q.ptr()))
			except("transaction.Query: block not allowed on request");
		// else do block with query
		Closer<SuQuery*> closer(val_cast<SuQuery*>(q));
		KEEPSP
		PUSH(q);
		return block.call(block, CALL, 1);
		}
	else if (member == QueryFirst)
		return queryone("transaction.QueryFirst", NEXT, false, this, args);
	else if (member == QueryLast)
		return queryone("transaction.QueryLast", PREV, false, this, args);
	else if (member == Query1)
		return queryone("transaction.Query1", NEXT, true, this, args);
	else if (member == UpdateQ)
		{
		NOARGS("transaction.Update?()");
		return tran % 2 ? SuTrue : SuFalse;
		}
	else if (member == EndedQ)
		{
		NOARGS("transaction.Ended?()");
		return done ? SuTrue : SuFalse;
		}
	else if (member == Complete)
		{
		NOARGS("transaction.Complete()");
		return commit() ? SuTrue : SuFalse;
		}
	else if (member == Rollback)
		{
		NOARGS("transaction.Rollback()");
		rollback();
		return Value();
		}
	else if (member == Conflict)
		{
		NOARGS("transaction.Conflict()");
		return new SuString(conflict ? conflict : "");
		}
	else if (member == ReadCount)
		{
		NOARGS("transaction.ReadCount()");
		return dbms()->readCount(tran);
		}
	else if (member == WriteCount)
		{
		NOARGS("transaction.ReadCount()");
		return dbms()->writeCount(tran);
		}
	else if (member == Data)
		{
		NOARGS("transaction.Data()");
		if (!data)
			data = new SuObject();
		return data;
		}
	else
		{
		static ushort G_Trans = globals("Transactions");
		Value Trans = globals.find(G_Trans);
		SuObject* ob;
		if (Trans && nullptr != (ob = Trans.ob_if_ob()) && ob->has(member))
			return ob->call(self, member, nargs, nargnames, argnames, each);
		else
			method_not_found("transaction", member);
		}
	}

Value SuTransaction::query(const char* s)
	{
	checkNotEnded("query");
	if (is_request(s))
		return dbms()->request(tran, s);
	else
		{
		DbmsQuery* q = dbms()->query(tran, s);
		if (! q)
			except("Transaction.Query: invalid query");
		return new SuQuery(gcstring(s), q, this);
		}
	}

bool SuTransaction::commit()
	{
	checkNotEnded("Complete");
	bool ok = dbms()->commit(tran, &conflict);
	done = true;
	return ok;
	}

void SuTransaction::rollback()
	{
	checkNotEnded("Rollback");
	dbms()->abort(tran);
	done = true;
	}

void SuTransaction::checkNotEnded(const char* action)
	{
	if (done)
		except("can't " << action << " ended Transaction" <<
			(*conflict ? " (" + gcstring(conflict) + ")" : ""));
	}

// SuCursor ------------------------------------------------------

Value CursorClass::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	if (member == CALL || member == INSTANTIATE)
		{
		if (nargs < 1 || nargs > 2)
			except("usage: Cursor(query [, block ])");
		gcstring s = ARG(0).gcstr();
		DbmsQuery* q = dbms()->cursor(s.str());
		if (! q)
			except("Cursor: invalid query");
		SuCursor* c = new SuCursor(s, q);
		if (nargs == 1)
			return c;
		// else do block with cursor
		Value block = ARG(1);
		Closer<SuCursor*> closer(c);
		KEEPSP
		PUSH(c);
		return block.call(block, CALL, 1);
		}
	else
		return RootClass::notfound(self, member, nargs, nargnames, argnames, each);
	}

int SuCursor::next_num = 0;

struct SetTran
	{
	SetTran(SuCursor* c, Value tran) : cursor(c)
		{
		cursor->t = force<SuTransaction*>(tran);
		cursor->q->set_transaction(cursor->t->tran);
		}
	~SetTran()
		{
		cursor->t = 0;
		cursor->q->set_transaction(NO_TRAN);
		}
	SuCursor* cursor;
	};

Value SuCursor::call(Value self, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value Next("Next");
	static Value Prev("Prev");
	static Value Output("Output");

	if (member == Next)
		{
		if (nargs != 1)
			except("usage: cursor.Next(transaction)");
		SetTran settran(this, ARG(0));
		return get(NEXT);
		}
	else if (member == Prev)
		{
		if (nargs != 1)
			except("usage: cursor.Prev(transaction)");
		SetTran settran(this, ARG(0));
		return get(PREV);
		}
	else if (member == Output)
		{
		if (nargs != 2)
			except("usage: cursor.Output(transaction, object)");
		SetTran settran(this, ARG(0));
		return output(ARG(1).object());
		}
	else
		return SuQuery::call(self, member, nargs, nargnames, argnames, each);
	}

// SuQuery ----------------------------------------------------------

SuQuery::SuQuery(const gcstring& s, DbmsQuery* n, SuTransaction* trans)
	: query(s), hdr(n->header()), q(n), t(trans), eof(NOT_EOF), closed(false)
	{
	}

// TODO: check if the controlling transaction is done

Value SuQuery::call(Value, Value member, 
	short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value NewRecord("NewRecord");
	static Value Keys("Keys");
	static Value Next("Next");
	static Value Prev("Prev");
	static Value Output("Output");
	static Value Columns("Columns");
	static Value Fields("Fields");
	static Value RuleColumns("RuleColumns");
	static Value Order("Order");
	static Value Rewind("Rewind");
	static Value Explain("Explain");
	static Value Close("Close");

	if (closed)
		except("cannot use closed query");
	if (member == NewRecord)
		{
		static Value gRecord = globals["Record"];
		return gRecord.call(gRecord, INSTANTIATE, nargs, nargnames, argnames, each);
		}
	else if (member == Keys)
		{
		NOARGS("query.Keys()");
		return getkeys();
		}
	else if (member == Next)
		{
		NOARGS("query.Next()");
		return get(NEXT);
		}
	else if (member == Prev)
		{
		NOARGS("query.Prev()");
		return get(PREV);
		}
	else if (member == Rewind)
		{
		NOARGS("query.Rewind()");
		return rewind();
		}
	else if (member == Columns || member == Fields) // Fields is deprecated
		{
		NOARGS("query.Columns()");
		return getfields();
		}
	else if (member == RuleColumns)
		{
		NOARGS("query.RuleColumns()");
		return getRuleColumns();
		}
	else if (member == Order)
		{
		NOARGS("query.Order()");
		return getorder();
		}
	else if (member == Explain)
		{
		NOARGS("query.Explain()");
		return explain();
		}
	else if (member == Output)
		{
		if (nargs != 1)
			except("usage: query.Output(object)");
		return output(TOP().object());
		}
	else if (member == Close)
		{
		NOARGS("query.Close()");
		close();
		return Value();
		}
	else
		method_not_found("query", member);
	}

Value SuQuery::get(Dir dir)
	{
	if (! t || t->isdone())
		except("cannot use a completed Transaction");
	if (eof == (dir == NEXT ? NEXT_EOF : PREV_EOF))
		return SuFalse;
	Row row = q->get(dir);
	if (row == Row::Eof)
		{
		eof = (dir == NEXT ? NEXT_EOF : PREV_EOF);
		return SuFalse;
		}
	eof = NOT_EOF;
	SuRecord* ob = new SuRecord(row, hdr, t);
	return ob;
	}

Value SuQuery::rewind()
	{
	eof = NOT_EOF;
	q->rewind();
	return 0;
	}

SuObject* SuQuery::getfields() const
	{
	SuObject* ob = new SuObject();
	for (Fields f = hdr.columns(); ! nil(f); ++f)
		if (! f->has_suffix("_deps"))
			ob->add(new SuString(*f));
	return ob;
	}

SuObject* SuQuery::getRuleColumns() const
	{
	SuObject* ob = new SuObject();
	for (Fields f = hdr.rules(); ! nil(f); ++f)
		ob->add(new SuString(*f));
	return ob;
	}

SuObject* SuQuery::getkeys() const
	{
	SuObject* ob = new SuObject();
	for (Lisp<Lisp<gcstring> > idxs = q->keys(); ! nil(idxs); ++idxs)
		{
		gcstring s;
		for (Fields idx = *idxs; ! nil(idx); ++idx)
			s += "," + *idx;
		ob->add(new SuString(s.substr(1)));
		}
	return ob;
	}

SuObject* SuQuery::getorder() const
	{
	SuObject* ob = new SuObject();
	for (Fields f = q->order(); ! nil(f); ++f)
		ob->add(new SuString(*f));
	return ob;
	}

Value SuQuery::explain() const
	{
	return new SuString(q->explain());
	}

Value SuQuery::output(SuObject* ob) const
	{
	Record r = object_to_record(hdr, ob);
	return q->output(r) ? SuTrue : SuFalse;
	}

Record object_to_record(const Header& hdr, SuObject* ob)
	{
	if (SuRecord* surec = dynamic_cast<SuRecord*>(ob))
		return surec->to_record(hdr);
	Record r;
	int ts = hdr.timestamp_field();
	Value tsval;
	for (Lisp<int> f = hdr.output_fldsyms(); ! nil(f); ++f)
		{
		if (*f == -1)
			r.addnil();
		else if (*f == ts)
			r.addval(tsval = dbms()->timestamp());
		else if (Value x = ob->getdata(symbol(*f)))
			r.addval(x);
		else
			r.addnil();
		}
	if (tsval && !ob->get_readonly())
		ob->put(symbol(ts), tsval);
	return r;
	}

void SuQuery::close()
	{
	if (! t || ! t->isdone())
		q->close();
	closed = true;
	}
