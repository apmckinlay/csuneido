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

#include "database.h"
#include "interp.h"
#include "globals.h"
#include "symbols.h"
#include "surecord.h"
#include "sudb.h"
#include "catstr.h"
#include "suboolean.h"

static Lisp<int> disabled_triggers;

void Tbl::user_trigger(int tran, const Record& oldrec, const Record& newrec)
	{
	if (tran == schema_tran)
		return ;
	if (! trigger)
		{
		trigger = globals(CATSTRA("Trigger_", name.str()));
		for (Fields f = get_fields(); ! nil(f); ++f)
			flds.push(*f == "-" ? -1 : symnum(f->str()));
		flds.reverse();
		}
	if (member(disabled_triggers, trigger))
		return ;
	Value fn = globals.find(trigger);
	if (! fn)
		return ;
	Value* oldsp = proc->stack.getsp();
	SuTransaction* t = new SuTransaction(tran);
	proc->stack.push(t);
	if (nil(oldrec))
		proc->stack.push(SuFalse);
	else
		proc->stack.push(new SuRecord(oldrec, flds, t));
	if (nil(newrec))
		proc->stack.push(SuFalse);
	else
		proc->stack.push(new SuRecord(newrec, flds, t));
	fn.call(fn, CALL, 3, 0, 0, -1);
	proc->stack.setsp(oldsp);
	}

static void pop(int n)
	{
	while (--n >= 0)
		disabled_triggers.pop();
	}

#include "prim.h"

Value su_do_without_triggers()
	{
	int nargs = 2;

	SuObject* ob = ARG(0).object();
	int n = 0;
	for (n = 0; ob->has(n); ++n)
		{
		char* table = ob->get(n).str();
		int trigger = globals(CATSTRA("Trigger_", table));
		disabled_triggers.push(trigger);
		}

	Value block = ARG(1);
	try
		{
		Value* sp = proc->stack.getsp();
		Value result = block.call(block, CALL, 0, 0, 0, -1);
		proc->stack.setsp(sp);
		pop(n);
		return result;
		}
	catch (const Except&)
		{
		pop(n);
		throw ;
		}
	}
PRIM(su_do_without_triggers, "DoWithoutTriggers(object, block)");
