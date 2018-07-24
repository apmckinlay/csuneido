#pragma once
// Copyright (c) 2017 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "win.h"
#include <cstdint>

inline int64_t qpc() {
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return t.QuadPart;
}

extern int64_t qpf; // defined in fibers.cpp
