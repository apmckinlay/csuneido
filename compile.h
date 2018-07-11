#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

class Value;
class CodeVisitor;

// compile a string of source code
Value compile(const char* s, const char* name = "", CodeVisitor* visitor = nullptr);
