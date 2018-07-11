// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "except.h"
#include "exceptimp.h"
#include "interp.h"
#include "ostreamstr.h"
#include "trace.h"
#include "suobject.h"
#include "sufunction.h"
#include "errlog.h"

SuObject* copyCallStack();

Except::Except(gcstring x) : SuString(x.trim()),
	fp_fn_(tls().proc ? tls().proc->fp->fn : 0), is_block_return(x == "block return")
	{
	if (x.has_prefix("block") &&
		(x == "block:continue" || x == "block:break" || is_block_return))
		{
		static SuObject* empty = new SuObject;
		calls_ =  empty;
		if (is_block_return)
			verify(fp_fn_);
		}
	else
		{
		TRACE(EXCEPTIONS, "throwing: " << x);
		calls_ = copyCallStack();
		}
	}

Except::Except(const Except& e, gcstring s)
	: SuString(s), fp_fn_(e.fp_fn_), calls_(e.calls_), is_block_return(e.is_block_return)
	{  }

Ostream& operator<<(Ostream& os, const Except& e)
	{
	return os << e.str();
	}

static OstreamStr os(200);

Ostream& osexcept()
	{
	return os;
	}

void except_()
	{
	Except e(os.str());
	os.clear();
	throw e;
	}

// to allow setting breakpoints
void except_err_()
	{
	Except e(os.str());
	os.clear();
	throw e;
	}

#define TRY(stuff) do try { stuff; } catch (...) { } while (false)

SuObject* copyCallStack()
	{
	static Value qqq = new SuString("???");
	SuObject* calls = new SuObject;
	if (! tls().proc || ! tls().proc->fp)
		return calls;
	for (Frame* f = tls().proc->fp; f > tls().proc->frames; --f)
		{
		SuObject* call = new SuObject;
		SuObject* vars = new SuObject;
		if (f->fn)
			{
			call->put("fn", f->fn);
			int i = 0, n = 0;
			TRY(i = f->fn->source(f->ip - f->fn->code - 1, &n));
			call->put("src_i", i);
			call->put("src_n", n);
			for (i = 0; i < f->fn->nlocals; ++i)
				if (f->local[i])
					{
					Value value = qqq;
					TRY(value = f->local[i]);
					TRY(vars->put(symbol(f->fn->locals[i]), value));
					}
			if (f->self)
				vars->put("this", f->self);
			}

		else if (f->prim)
			{
			TRY(call->put("fn", f->prim));
			}

		call->put("locals", vars);
		calls->add(call);
		}
	return calls;
	}

// used for "function call overflow" and "value stack overflow"
void except_log_stack_()
	{
	static bool first = true;
	const char* stk = "";
	if (first)
		{
		first = false;
		stk = callStackString();
		}
	errlog(os.str(), stk);
	except_();
	}

char* callStackString()
	{
	OstreamStr stk;
	gcstring indent = "";
	for (Frame* f = tls().proc->fp; f > tls().proc->frames; --f)
		{
		stk << endl << indent;
		if (f->fn)
			stk << f->fn;
		else if (f->prim)
			stk << f->prim;
		indent += "  ";
		}
	return stk.str();
	}

Value Except::call(Value self, Value member, 
	short nargs, short nargnames, short* argnames, int each)
	{
	static Value As("As");
	static Value Callstack("Callstack");
	if (member == As)
		{
		argseach(nargs, nargnames, argnames, each);
		if (nargs != 1)
			except("usage: exception.As(string)");
		return new Except(*this, ARG(0).gcstr());
		}
	if (member == Callstack)
		{
		argseach(nargs, nargnames, argnames, each);
		NOARGS("exception.Callstack()");
		return calls();
		}
	else
		return SuString::call(self, member, nargs, nargnames, argnames, each);
	}

char* Except::callstack() const
	{
	OstreamStr oss;
	for (int i = 0; i < calls_->size(); ++i)
		oss << endl << calls_->get(i).getdata("fn");
	return oss.str();
	}
