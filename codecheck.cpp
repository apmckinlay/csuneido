/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2007 Suneido Software Corp. 
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

#include "codecheck.h"
#include "codevisitor.h"
#include "except.h"
#include "suobject.h"

#include <vector>
#include <stack>

// TODO: uninitialized locals
// TODO: private members only used once
// TODO: uninitialized private members (by definition or assignment)

struct Local
	{
	explicit Local(int p = 0) : init(false), used(false), pos(p)
		{ }
	bool init;
	bool used;
	int pos;
	};

typedef std::vector<Local> Locals;

struct Event
	{
	Event(int p = -1, int v = -1, bool in = false) : pos(p), var(v), init(in)
		{ }
	int pos;
	int var;
	bool init;
	};

class CodeCheck : public CodeVisitor
	{
public:
	CodeCheck(SuObject* ob) : results(ob)
		{ }
	virtual void local(int pos, int i, bool init);
	virtual void global(int pos, int gnum);
	virtual void begin_func();
	virtual void end_func();
private:
	void process(int pos, int i, bool init);
	std::stack<Locals> stack;
	SuObject* results;
	Event event;
	};

CodeVisitor* make_codecheck(SuObject* results)
	{
	return new CodeCheck(results);
	}

// complicated because compiler sometimes calls local
// before knowing if it's an initialization
// in which cases it calls local again to fix up
void CodeCheck::local(int pos, int var, bool init)
	{
	if (event.pos != -1)
		{
		// ignore previous event if this is a "fixup"
		if (var != event.var || ! init || event.init)
			process(event.pos, event.var, event.init);
		}
	event = Event(pos, var, init);
	}

void CodeCheck::process(int pos, int var, bool init)
	{
	verify(stack.size() > 0);
	Locals& top = stack.top();
	
	if (var == top.size())
		top.push_back(Local(pos));
	else if (var >= top.size())
		except("CodeCheck::local unexpected index");
	Local& local = top[var];
	if (init)
		{
		if (! local.init)
			local.pos = pos; // previous positions already added to results
		local.init = true;
		}
	else
		{
		if (local.init)
			local.used = true;
		else
			results->add(pos); // use without init
		}
	}

void CodeCheck::begin_func()
	{
	if (event.pos != -1)
		process(event.pos, event.var, event.init);
	event = Event();
	
	stack.push(Locals()); 
	}

void CodeCheck::end_func()
	{
	if (event.pos != -1)
		process(event.pos, event.var, event.init);
	event = Event();
	
	verify(stack.size() > 0);
	Locals& top = stack.top();
	for (int i = 0; i < top.size(); ++i)
		if (top[i].init && ! top[i].used)
			results->add(top[i].pos);
	stack.pop();
	}

#include "globals.h"

void CodeCheck::global(int pos, int gnum)
	{
	if (*globals(gnum) == '_'  || ! globals.find(gnum))
		results->add(pos);
	}
