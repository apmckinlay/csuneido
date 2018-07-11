#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostream.h"

class gcstring;
class Buffer;

// an output stream to a string
class OstreamStr : public Ostream
	{
public:
	explicit OstreamStr(int len = 32);
	Ostream& write(const void* buf, int n) override;
	char* str();
	gcstring gcstr();
	void clear();
	int size() const;
private:
	Buffer* buf;
	};

#define OSTR(stuff) ((OstreamStr&) (OstreamStr() << "" << stuff)).str()
