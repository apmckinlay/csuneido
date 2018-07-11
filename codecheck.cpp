// Copyright (c) 2007 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "codecheck.h"
#include "codevisitor.h"
#include "except.h"
#include "suobject.h"

#include <vector>
#include <stack>

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
	void local(int pos, int var, bool init) override;
	void dynamic(int var) override;
	void global(int pos, const char* name) override;
	void begin_func() override;
	void end_func() override;
private:
	void process(int pos, int i, bool init);
	void setupLocal(int var);

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
// in which cases it calls local again to undo
void CodeCheck::local(int pos, int var, bool init)
	{
	if (event.pos != -1)
		{
		if (pos == -1)
			{
			verify(var == event.var);
			verify(!event.init);
			setupLocal(var);
			event.pos = -1; // wipe out previous event	
			return;
			}
		process(event.pos, event.var, event.init);
		}
	event = Event(pos, var, init);
	}

void CodeCheck::dynamic(int var)
	{
	if (event.pos != -1)
		process(event.pos, event.var, event.init);
	// mark _name as both used and initialized
	process(0, var, true);
	process(0, var, false);
	event = Event();
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
			results->add(pos); // use without init - ERROR
		}
	}

void CodeCheck::setupLocal(int var)
	{
	Locals& top = stack.top();
	if (var == top.size())
		top.push_back(Local(event.pos));
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
			results->add(-top[i].pos); // init but not used - WARNING
	stack.pop();
	}

#include "globals.h"

void CodeCheck::global(int pos, const char* name)
	{
	if (*name == '_')
		results->add(-pos); // _Name where Name is defined - WARNING
		// Compiler throws before getting here if Name is undefined
	else
		try
			{
			if (! globals.find(name))
				results->add(pos); // reference to undefined global - ERROR
			}
		catch (...)
			{
			results->add(-pos); // error compiling referenced global - WARNING
			}
	}
