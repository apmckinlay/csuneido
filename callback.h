#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "type.h"
#include "named.h"

// a Type for defining calls from external code
class Callback : public TypeMulti {
public:
	NAMED
	Callback(TypeItem* it, short* ns, short n) : TypeMulti(it, n) {
		mems = dup(ns, n);
	}
	int size() override {
		return sizeof(void*);
	}
	void put(char*& dst, char*& dst2, const char* lim2, Value x) override;
	Value get(const char*& src, Value x) override {
		src += sizeof(void*);
		return x;
	}
	void out(Ostream& os) const override;
	int callback(Value fn, const char* src);

private:
	short* mems;
};
