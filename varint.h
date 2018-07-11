#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <vector>
using namespace std;
#include "std.h"

extern void push_varint(vector<uint8_t>& code, int n);

extern int varint(uint8_t* code, int& ci);
extern int varint(uint8_t*& code);
