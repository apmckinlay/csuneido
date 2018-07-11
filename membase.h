#pragma once
// Copyright (c) 2017 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "meth.h"
#include "named.h"
#include "value.h"
#include "htbl.h"

class MemBase : public SuValue
	{
public:
	NAMED
	MemBase() : named(".")
		{ }

private:
	template <typename Finder> Value lookup(Finder finder);
	void addMembersTo(SuObject* ob);
	Value method_lookup(BuiltinArgs& args);

protected:
	Value Base(BuiltinArgs& args);
	bool hasBase(Value value);
	Value BaseQ(BuiltinArgs& args);
	Value Eval(BuiltinArgs& args);
	Value Eval2(BuiltinArgs& args);
	Value GetDefault(BuiltinArgs& args);
	Value MemberQ(BuiltinArgs& args);
	Value Members(BuiltinArgs& args);
	Value MethodQ(BuiltinArgs& args);
	Value MethodClass(BuiltinArgs& args);
	virtual Value ReadonlyQ(BuiltinArgs& args);
	Value Size(BuiltinArgs& args);

	Value method_class(Value m);
	static MemFun<MemBase> method(Value member);
	Value callSuper(Value self, Value member, short nargs, short nargnames, short* argnames, int each);
	virtual Value mbclass() = 0; // SuInstance.myclass or SuClass this
	virtual Value parent() = 0; // SuInstance.myclass or SuClass.base
	virtual bool readonly() = 0;

	Hmap<Value, Value> data;
	};
