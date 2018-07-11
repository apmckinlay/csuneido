#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "istream.h"

class IstreamStrImp;

// an input stream from a string
class IstreamStr : public Istream
	{
public:
	explicit IstreamStr(const char* s);
	IstreamStr(const char* buf, int n);
	int tellg() override;
	Istream& seekg(int pos) override;
protected:
	int get_() override;
	int read_(char* buf, int n) override;
private:
	IstreamStrImp* imp;
	};
