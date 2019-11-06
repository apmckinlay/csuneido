#pragma once
// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostream.h"

extern Ostream& cout;
extern Ostream& cerr;

#define SHOW(stuff) cout << #stuff << " => " << (stuff) << endl
