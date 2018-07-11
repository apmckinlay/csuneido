#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

class Database* theDB();

class TranCloser
	{
public:
	explicit TranCloser(int t_) : t(t_)
		{ }
	operator int() const
		{ return t; }
	~TranCloser();
private:
	int t;
	};
