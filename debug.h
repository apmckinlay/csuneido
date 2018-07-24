#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostream.h"

Ostream& osdebug();
void debug_();

#define dbg(msg) ((osdebug() << msg), debug_())
