#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "suvalue.h"

// base for Windows resources - SuHandle and SuGdiObj
class SuWinRes : public SuValue {
public:
	explicit SuWinRes(void* handle);
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
	void* handle() const {
		return h;
	}

protected:
	virtual bool close() = 0;
	void* h;
};

// a value type to hold Windows HANDLE's
class SuHandle : public SuWinRes {
public:
	explicit SuHandle(void* handle);
	void out(Ostream& os) const override;
	bool close() override;
};

// a value type to hold Windows GDIOBJ's
class SuGdiObj : public SuWinRes {
public:
	explicit SuGdiObj(void* handle);
	void out(Ostream& os) const override;
	int integer() const override;
	bool close() override;
};
