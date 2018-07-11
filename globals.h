#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"

typedef unsigned short uint16_t;

// global definitions - builtin and library
struct Globals
	{
	uint16_t operator()(const char* s);
	uint16_t copy(const char* s);	// for class : _Base
	char* operator()(uint16_t j);
	Value operator[](const char* s);
	Value operator[](uint16_t j);
	Value find(const char* s)
		{ return find(operator()(s)); }
	Value find(uint16_t j);
	Value get(uint16_t j);
	Value get(const char* s)
		{ return get(operator()(s)); }
	void put(const char* s, Value x)
		{ put(operator()(s), x); }
	void put(uint16_t j, Value x);
	void clear();
	void pop(uint16_t i);
	};

extern Globals globals;
