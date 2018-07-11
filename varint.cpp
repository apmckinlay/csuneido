// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// NOTE: only for positive numbers (doesn't zigzag encode)

#include "varint.h"
#include "except.h"

void push_varint(vector<uint8_t>& code, int n)
	{
	verify(n >= 0);
	while (n > 0x7f)
		{
		code.push_back((n & 0x7f) | 0x80);
		n >>= 7;
		}
	code.push_back(n & 0x7f);
	}

int varint(uint8_t* code, int& ci)
	{
	int shift = 0;
	int n = 0;
	int b;
	do
		{
		b = code[ci++];
		n |= (b & 0x7f) << shift;
		shift += 7;
		} while (b & 0x80);
	return n;
	}

int varint(uint8_t*& code)
	{
	int shift = 0;
	int n = 0;
	int b;
	do
		{
		b = *code++;
		n |= (b & 0x7f) << shift;
		shift += 7;
		} while (b & 0x80);
	return n;
	}

// tests ------------------------------------------------------------

#include "testing.h"

static void test(int n)
	{
	vector<uint8_t> code;
	push_varint(code, n);
	int ci = 0;
	assert_eq(varint(&code[0], ci), n);
	}

TEST(varint)
	{
	test(0);
	test(99);
	test(1234);
	test(12345);
	}
