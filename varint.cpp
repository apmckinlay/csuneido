// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// NOTE: only for positive numbers (doesn't zigzag encode)

#include "varint.h"
#include "except.h"

// append a varint to a byte vector
void push_varint(vector<uint8_t>& code, int n) {
	verify(n >= 0);
	while (n > 0x7f) {
		code.push_back((n & 0x7f) | 0x80);
		n >>= 7;
	}
	code.push_back(n & 0x7f);
}

// return a varint at a position, advancing the position
int varint(uint8_t* code, int& ci) {
	int shift = 0;
	int n = 0;
	int b;
	do {
		b = code[ci++];
		n |= (b & 0x7f) << shift;
		shift += 7;
	} while (b & 0x80);
	return n;
}

// return a varint from a pointer, advancing the pointer
int varint(uint8_t*& code) {
	int shift = 0;
	int n = 0;
	int b;
	do {
		b = *code++;
		n |= (b & 0x7f) << shift;
		shift += 7;
	} while (b & 0x80);
	return n;
}

#pragma intrinsic(_BitScanReverse)

// return the number of bytes required
int varintSize(unsigned long n) {
	if (n == 0)
		return 1;
	unsigned long pos;
	_BitScanReverse(&pos, n);
	auto bits = pos + 1;
	return (bits + 6) / 7;
}

// append a varint to dst, returning the number of bytes used
int uvarint(char* dst, unsigned int n) {
	int i = 1;
	while (n > 0x7f) {
		*dst++ = (n & 0x7f) | 0x80;
		n >>= 7;
		++i;
	}
	*dst++ = n & 0x7f;
	return i;
}

// return a varint, advancing src
int uvarint(const char*& src) {
	int shift = 0;
	int n = 0;
	int b;
	do {
		b = *src++;
		n |= (b & 0x7f) << shift;
		shift += 7;
	} while (b & 0x80);
	return n;
}

// tests ------------------------------------------------------------

#include "testing.h"

static void test(int n) {
	vector<uint8_t> code;
	push_varint(code, n);
	int ci = 0;
	assert_eq(varint(&code[0], ci), n);
}

TEST(varint) {
	test(0);
	test(99);
	test(1234);
	test(12345);
}

TEST(varintsize) {
	assert_eq(varintSize(0), 1);
	assert_eq(varintSize(123), 1);
	assert_eq(varintSize(234), 2);
	assert_eq(varintSize(0xffffffff), 5);
}
