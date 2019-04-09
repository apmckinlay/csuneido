#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include <vector>
using namespace std;
#include "std.h"

void push_varint(vector<uint8_t>& code, int n);

int varint(uint8_t* code, int& ci);
int varint(uint8_t*& code);

int varintSize(unsigned long n);
int uvarint(char* dst, unsigned int n);
int uvarint(const char*& src);
