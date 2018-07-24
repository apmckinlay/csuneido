#pragma once
// Copyright (c) 2004 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "gcstring.h"
#include "hashmap.h"

class SesViews : public HashMap<gcstring, gcstring> {};

void set_session_view(const gcstring& name, const gcstring& def);

gcstring get_session_view(const gcstring& name);

void remove_session_view(const gcstring& name);
