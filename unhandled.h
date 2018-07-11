#pragma once
// Copyright (c) 2005 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

void unhandled();
char* err_filename();

[[noreturn]] void fatal_log(const char* error, const char* extra = "");
