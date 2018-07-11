// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "database.h"
#include "interp.h"
#include "globals.h"
#include "symbols.h"
#include "surecord.h"
#include "sudb.h"
#include "catstr.h"
#include "exceptimp.h"

extern bool is_client;

static Lisp<int> disabled_triggers;

void Tbl::user_trigger(int tran, const Record& oldrec, const Record& newrec)
	{
	if (tran == schema_tran)
		return ;
	if (trigger == -1)
		return ;
	Value fn;
	if (! trigger)
		{
		trigger = globals(CATSTRA("Trigger_", name.str()));
		fn = globals.find(trigger);
		if (! fn)
			{
			globals.pop(trigger); // remove it if we just added it
			trigger = -1;
			return ;
			}
		for (Fields f = get_fields(); ! nil(f); ++f)
			flds.push(*f == "-" ? -1 : symnum(f->str()));
		flds.reverse();
		}
	else
		{
		fn = globals.find(trigger);
		if (! fn)
			return ;
		}
	if (member(disabled_triggers, trigger))
		return ;
	KEEPSP
	SuTransaction* t = new SuTransaction(tran);
	PUSH(t);
	PUSH(nil(oldrec) ? SuFalse : new SuRecord(oldrec, flds, t));
	PUSH(nil(newrec) ? SuFalse : new SuRecord(newrec, flds, t));
	try
		{
		fn.call(fn, CALL, 3);
		}
	catch (const Except& e)
		{
		throw Except(e, e.gcstr() + " (" + globals(trigger) + ")");
		}
	}

struct DisabledTriggers
	{
	DisabledTriggers() : n(0)
		{ }
	void push(int t)
		{
		disabled_triggers.push(t);
		++n;
		}
	~DisabledTriggers()
		{
		while (n-- > 0)
			disabled_triggers.pop();
		}
	int n;
	};

#include "builtin.h"

BUILTIN(DoWithoutTriggers, "(object, block)")
	{
	int nargs = 2;

	if (is_client)
		except("DoWithoutTriggers cannot be used when running as a client");
	SuObject* ob = ARG(0).object();
	DisabledTriggers dt;
	for (int i = 0; ob->has(i); ++i)
		{
		auto table = ob->get(i).str();
		int trigger = globals(CATSTRA("Trigger_", table));
		dt.push(trigger);
		}

	KEEPSP
	Value block = ARG(1);
	return block.call(block, CALL);
	}
