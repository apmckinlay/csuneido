#pragma once
// Copyright (c) 2009 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "sustring.h"

class SuObject;
class Frame;
class SuFunction;

class Except : public SuString
	{
public:
	explicit Except(gcstring s);
	Except(const Except& e, gcstring s);
	Value call(Value self, Value member, 
		short nargs, short nargnames, short* argnames, int each) override;
	SuObject* calls() const
		{ return calls_; }
	SuFunction* fp_fn() const
		{ return fp_fn_; }
	bool isBlockReturn() const
		{ return is_block_return; }
	char* callstack() const;
private:
	SuFunction* fp_fn_;
	SuObject* calls_;
	bool is_block_return;
	};
