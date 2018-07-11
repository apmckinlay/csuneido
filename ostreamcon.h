#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostream.h"

class OstreamConImp;

// an output stream to a file, using stdio
class OstreamCon : public Ostream
	{
public:
	OstreamCon();
	~OstreamCon();
	Ostream& write(const void* buf, int n) override;
	explicit operator bool() const;
private:
	OstreamConImp* imp;
	};

extern Ostream& con();
