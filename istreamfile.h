#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "istream.h"

class IstreamFileImp;

// an input stream from a file, using stdio
class IstreamFile : public Istream {
public:
	explicit IstreamFile(const char* filename, const char* mode = "r");
	~IstreamFile();
	int tellg() override;
	Istream& seekg(int pos) override;
	int size() const;
	void close() const;

protected:
	int get_() override;
	int read_(char* buf, int n) override;

private:
	IstreamFileImp* imp;
};
