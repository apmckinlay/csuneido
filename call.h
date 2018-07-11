#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "lisp.h"
#include "value.h"

Value call(const char* fname, Lisp<Value> args);
Value method_call(Value ob, const gcstring& method, Lisp<Value> args);
const char* trycall(const char* fn, char* arg, int* plen = 0);
