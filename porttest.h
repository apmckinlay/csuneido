#pragma once
// Copyright (c) 2018 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostream.h"
#include "list.h"
#include "gcstring.h"
#include "testing.h"

using PTfn = const char* (*)(const List<gcstring>& args, const List<bool>& str);

struct PortTest
	{
	PortTest(const char* name, PTfn fn);
	static void run(Testing& t, const char* prefix);

	const char* name;
	PTfn fn;
	};

#define PORTTEST(name) \
	const char* pt_##name(const List<gcstring>& args, const List<bool>& str); \
	PortTest pti_##name(#name, pt_##name); \
	const char* pt_##name(const List<gcstring>& args, const List<bool>& str)
