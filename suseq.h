#pragma once
// Copyright (c) 2002 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"

class SuObject;

class SuSeq : public SuValue {
public:
	explicit SuSeq(Value iter);
	void out(Ostream& os) const override;
	Value call(Value self, Value member, short nargs, short nargnames,
		short* argnames, int each) override;
	void putdata(Value i, Value x) override;
	Value getdata(Value) override;
	Value rangeTo(int i, int j) override;
	Value rangeLen(int i, int n) override;
	size_t packsize() const override;
	void pack(char* buf) const override;
	bool lt(const SuValue& y) const override;
	bool eq(const SuValue& y) const override;
	SuObject* ob_if_ob() override;
	SuObject* object() const;
	const char* type() const override {
		return "Object";
	}

private:
	Value Join(short nargs) const;
	static bool infinite(Value it);
	void build() const;
	static SuObject* copy(Value iter);
	Value dup() const;
	static Value next(Value iter);

	mutable Value iter;
	mutable SuObject* ob = nullptr;
	mutable bool duped = false;
};
