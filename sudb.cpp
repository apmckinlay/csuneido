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
#include "suclass.h"
#include "surecord.h"
#include "sustring.h"
#include "interp.h"
#include "dbms.h"
#include "except.h"
#include "exceptimp.h"
#include "suboolean.h"
#include "pack.h"
#include "symbols.h"
#include "globals.h"
#include "query.h" // for is_request
#include "ostreamstr.h"
#include "func.h"
#include "builtinargs.h"
#include "trace.h"

Value DatabaseClass::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	static Value Transactions("Transactions");
	static Value CurrentSize("CurrentSize");
	static Value Connections("Connections");
	static Value TempDest("TempDest");
	static Value Cursors("Cursors");
	static Value SessionId("SessionId");
	static Value Final("Final");
	static Value Kill("Kill");
	static Value Dump("Dump");
	static Value Load("Load");
	static Value Nonce("Nonce");
	static Value Token("Token");
	static Value Auth("Auth");
	static Value Check("Check");

	argseach(nargs, nargnames, argnames, each);

	if (member == CALL || member == INSTANTIATE)
		{
		if (nargs != 1)
			except("usage: Database(request)");
		dbms()->admin(ARG(0).str());
		return SuTrue;
		}
	else if (member == Transactions)
		{
		SuObject* ob = new SuObject();
		for (Lisp<int> trans = dbms()->tranlist(); ! nil(trans); ++trans)
			ob->add(*trans);
		return ob;
		}
	else if (member == CurrentSize)
		{
		if (nargs != 0)
			except("usage: Database.CurrentSize()");
		return SuNumber::from_int64(dbms()->size());
		}
	else if (member == Connections)
		{
		if (nargs != 0)
			except("usage: Database.Connections()");
		return dbms()->connections();
		}
	else if (member == TempDest)
		{
		if (nargs != 0)
			except("usage: Database.TempDest()");
		return dbms()->tempdest();
		}
	else if (member == Cursors)
		{
		if (nargs != 0)
			except("usage: Database.Cursors()");
		return dbms()->cursors();
		}
	else if (member == SessionId)
		{
		char *s = "";
		if (nargs == 1)
			s = ARG(0).str();
		else if (nargs != 0)
			except("usage: Database.SessionId(newid = '')");
		return dbms()->sessionid(s);
		}
	else if (member == Final)
		{
		if (nargs != 0)
			except("usage: Database.Final()");
		return dbms()->final();
		}
	else if (member == Kill)
		{
		if (nargs != 1)
			except("usage: Database.Kill(session_id)");
		return dbms()->kill(ARG(0).str());
		}
	else if (member == Dump)
		{
		char* what;
		if (nargs == 0)
			what = "";
		else if (nargs == 1)
			what = ARG(0).str();
		else
			except("usage: Database.Dump(table = '')");
		Value result = dbms()->dump(what);
		if (result != SuEmptyString)
			except("Database.Dump failed: " << result);
		return Value();
		}
	else if (member == Load)
		{
		char* what;
		if (nargs == 1)
			what = ARG(0).str();
		else
			except("usage: Database.Load(table)");
		int result = dbms()->load(what);
		if (result < 0)
			except("Database.Load failed: " << result);
		return result;
		}
	else if (member == Token)
		{
		if (nargs != 0)
			except("usage: Database.Token()");
		return new SuString(dbms()->token());
		}
	else if (member == Nonce)
		{
		if (nargs != 0)
			except("usage: Database.Nonce()");
		return new SuString(dbms()->nonce());
		}
	else if (member == Auth)
		{
		if (nargs != 1)
			except("usage: Database.Auth(string)");
		return dbms()->auth(ARG(0).gcstr()) ? SuTrue : SuFalse;
		}
	else if (member == Check)
		{
		if (nargs != 0)
			except("usage: Database.Check()");
		return dbms()->check();
		}
	else
		return RootClass::notfound(self, member, nargs, nargnames, argnames, each);
	}

static char* query_args(char* query, BuiltinArgs& args)
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

static char* traceTran(SuTransaction* tran)
	{
	if (! tran)
		return "";
	OstreamStr os;
	os << 'T' << tran->tran << ' ';
	return os.str();
	}

#define QUERYONE_PARAMS "(query [, field: value ...])"

static Value queryone(char* which, Dir dir, bool one, SuTransaction* tran, BuiltinArgs& args)
	{
	if (tran)
		tran->checkNotEnded("query");
	args.usage("usage: ", which, QUERYONE_PARAMS);
	char* query = args.getstr("query");
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
	QueryOne(char* w, Dir d, bool o) : which(w), dir(d), one(o)
		{
		named.num = globals("Query1");
		}
	Value call(Value self, Value member,
		short nargs, short nargnames, ushort* argnames, int each)
		{
		static Value Params("Params");

		BuiltinArgs args(nargs, nargnames, argnames, each);
		if (member == CALL)
			return queryone(which, dir, one, NULL, args);
		else if (member == Params)
			{
			args.usage("usage: ", which, ".Params()").end();
			return new SuString(QUERYONE_PARAMS);
			}
		else
			method_not_found(which, member);
		}
	virtual const char* type() const
		{ return "Builtin"; }
	void out(Ostream& os)
		{ os << which << " /* function */"; }
private:
	char* which;
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

Value TransactionClass::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
	{
	if (member == CALL || member == INSTANTIATE)
		{
		static ushort read = ::symnum("read");
		static ushort update = ::symnum("update");
		if (nargs < 1 || nargs > 2 || nargnames < 1 ||
			(argnames[0] != read && argnames[0] != update))
			except("usage: Transaction(read: [, block ]) or Transaction(update: [, block ])");
		SuTransaction* t = new SuTransaction(
			argnames[0] == update && ARG(0) == SuTrue
				? SuTransaction::READWRITE : SuTransaction::READONLY);
		if (nargs == 1)
			return t;
		// else do block with transaction
		Value block = ARG(1);
		try
			{
			KEEPSP
			PUSH(t);
			Value result = block.call(block, CALL, 1, 0, 0, -1);
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

SuTransaction::SuTransaction(TranType type) : done(false), conflict("")
	{
	tran = dbms()->transaction((Dbms::TranType) type);
	}

SuTransaction::SuTransaction(int t) : tran(t), done(false), conflict("")
	{ verify(t > 0); }

Value SuTransaction::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
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

	BuiltinArgs args(nargs, nargnames, argnames, each);
	if (member == Query)
		{
		args.usage("usage: transaction.Query(value [, block][, field: value...])");
		char* qstr = args.getstr("query");
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
		return block.call(block, CALL, 1, 0, 0, -1);
		}
	else if (member == QueryFirst)
		return queryone("transaction.QueryFirst", NEXT, false, this, args);
	else if (member == QueryLast)
		return queryone("transaction.QueryLast", PREV, false, this, args);
	else if (member == Query1)
		return queryone("transaction.Query1", NEXT, true, this, args);
	else if (member == UpdateQ)
		{
		if (nargs != 0)
			except("usage: transaction.Update?()");
		return tran % 2 ? SuTrue : SuFalse;
		}
	else if (member == EndedQ)
		{
		if (nargs != 0)
			except("usage: transaction.Ended?()");
		return done ? SuTrue : SuFalse;
		}
	else if (member == Complete)
		{
		if (nargs != 0)
			except("usage: transaction.Complete()");
		return commit() ? SuTrue : SuFalse;
		}
	else if (member == Rollback)
		{
		if (nargs != 0)
			except("usage: transaction.Rollback()");
		rollback();
		return Value();
		}
	else if (member == Conflict)
		{
		if (nargs != 0)
			except("usage: transaction.Conflict()");
		return new SuString(conflict ? conflict : "");
		}
	else if (member == ReadCount)
		{
		if (nargs != 0)
			except("usage: transaction.ReadCount()");
		return dbms()->readCount(tran);
		}
	else if (member == WriteCount)
		{
		if (nargs != 0)
			except("usage: transaction.ReadCount()");
		return dbms()->writeCount(tran);
		}
	else
		{
		static ushort G_Trans = globals("Transactions");
		Value Trans = globals.find(G_Trans);
		SuObject* ob;
		if (Trans && (ob = Trans.ob_if_ob()) && ob->has(member))
			return ob->call(self, member, nargs, nargnames, argnames, each);
		else
			method_not_found("transaction", member);
		}
	}

Value SuTransaction::query(char* s)
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

void SuTransaction::checkNotEnded(char* action)
	{
	if (done)
		except("can't " << action << " ended Transaction" <<
			(*conflict ? " (" + gcstring(conflict) + ")" : ""));
	}

// SuCursor ------------------------------------------------------

Value CursorClass::call(Value self, Value member, short nargs, short nargnames, ushort* argnames, int each)
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
		return block.call(block, CALL, 1, 0, 0, -1);
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

Value SuCursor::call(Value self, Value member, short nargs,
	short nargnames, ushort* argnames, int each)
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

Value SuQuery::call(Value, Value member, short nargs, short nargnames, ushort* argnames, int each)
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
		if (nargs != 0)
			except("usage: query.Keys()");
		return getkeys();
		}
	else if (member == Next)
		{
		if (nargs != 0)
			except("usage: query.Next()");
		return get(NEXT);
		}
	else if (member == Prev)
		{
		if (nargs != 0)
			except("usage: query.Prev()");
		return get(PREV);
		}
	else if (member == Rewind)
		{
		if (nargs != 0)
			except("usage: query.Rewind()");
		return rewind();
		}
	else if (member == Columns || member == Fields) // Fields is deprecated
		{
		if (nargs != 0)
			except("usage: query.Columns()");
		return getfields();
		}
	else if (member == RuleColumns)
		{
		if (nargs != 0)
			except("usage: query.RuleColumns()");
		return getRuleColumns();
		}
	else if (member == Order)
		{
		if (nargs != 0)
			except("usage: query.Order()");
		return getorder();
		}
	else if (member == Explain)
		{
		if (nargs != 0)
			except("usage: query.Explain()");
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
		if (nargs != 0)
			except("usage: query.Close()");
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

SuObject* SuQuery::getfields()
	{
	SuObject* ob = new SuObject();
	for (Fields f = hdr.columns(); ! nil(f); ++f)
		if (! f->has_suffix("_deps"))
			ob->add(new SuString(*f));
	return ob;
	}

SuObject* SuQuery::getRuleColumns()
	{
	SuObject* ob = new SuObject();
	for (Fields f = hdr.rules(); ! nil(f); ++f)
		ob->add(new SuString(*f));
	return ob;
	}

SuObject* SuQuery::getkeys()
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

SuObject* SuQuery::getorder()
	{
	SuObject* ob = new SuObject();
	for (Fields f = q->order(); ! nil(f); ++f)
		ob->add(new SuString(*f));
	return ob;
	}

Value SuQuery::explain()
	{
	return new SuString(q->explain());
	}

Value SuQuery::output(SuObject* ob)
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
	for (Lisp<int> f = hdr.output_fldsyms(); ! nil(f); ++f)
		{
		if (*f == -1)
			r.addnil();
		else if (*f == ts)
			r.addval(dbms()->timestamp());
		else if (Value x = ob->getdata(symbol(*f)))
			r.addval(x);
		else
			r.addnil();
		}
	return r;
	}

void SuQuery::close()
	{
	if (! t || ! t->isdone())
		q->close();
	closed = true;
	}
