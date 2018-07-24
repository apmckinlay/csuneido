#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostream.h"

class OstreamFileImp;

// an output stream to a file, using stdio
class OstreamFile : public Ostream {
public:
	explicit OstreamFile(const char* filename, const char* mode = "w");
	~OstreamFile();
	Ostream& write(const void* buf, int n) override;
	explicit operator bool() const;
	void flush() override;

private:
	OstreamFileImp* imp;
};
