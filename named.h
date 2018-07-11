#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "gcstring.h"

struct Named
	{
	explicit Named(const char* c = " ") : separator(c)
		{ }
	gcstring name() const;
	gcstring library() const;
	gcstring info() const; // for debugging

	gcstring lib;
	const Named* parent = nullptr;
	const char* separator;
	uint16_t num = 0; // either global index or symnum
	const char* str = nullptr; // alternative to num to avoid symbol
	};

// place NAMED in the public portion of a class to implement naming

#define NAMED \
	Named named; \
	const Named* get_named() const override \
		{ return &named; }
