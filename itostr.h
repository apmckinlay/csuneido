#pragma once
// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "std.h"

char* itostr(int x, char* buf, int radix = 10);
char* utostr(unsigned int x, char* buf, int radix = 10);

const int I64BUF = 32;
char* i64tostr(int64_t x, char* buf, int radix = 10);
char* u64tostr(uint64_t x, char* buf, int radix = 10);
