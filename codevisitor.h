#pragma once
// Copyright (c) 2007 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

class CodeVisitor
	{
public:
	virtual void local(int pos, int var, bool init)
		{ }
	virtual void dynamic(int var)
		{ }
	virtual void global(int pos, const char* name)
		{ }
	virtual void begin_func()
		{ }
	virtual void end_func()
		{ }
	virtual ~CodeVisitor() = default;
	};
