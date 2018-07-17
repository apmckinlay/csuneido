#pragma once
// Copyright (c) 2006 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "gcstring.h"
#include "ostream.h"

void circ_log(const gcstring& s);
gcstring circ_log_get();

Ostream& oscirclog();
void circlogos_();

#define CIRCLOG(stuff) \
	((oscirclog() << stuff), circlogos_())  // NOLINT(misc-macro-parentheses)
