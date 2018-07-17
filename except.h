#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostream.h"
// ReSharper disable once CppUnusedIncludeDirective
#include <cstring> // for strrchr

[[noreturn]] void except_();
[[noreturn]] void except_err_();
[[noreturn]] void except_log_stack_();
char* callStackString();

Ostream& osexcept();

#define except(msg) \
	((osexcept() << msg), except_())  // NOLINT

#define except_err(msg) \
	((osexcept() << msg), except_err_())  // NOLINT

#define except_if(e, msg) \
	((e) ? except_err(msg) : (void) 0)

#define FILE__ (strrchr("\\" __FILE__, '\\') + 1)

#define error(msg) \
	except_err(FILE__ << ':' << __LINE__ << ": ERROR: " << msg)  // NOLINT

#define except_log_stack(msg) \
	((osexcept() << msg), except_log_stack_())  // NOLINT

#define verify(e) \
	((e) ? (void) 0 : error("assert failed: " << #e))

#define unreachable()	error("should not reach here!")

class Except;

Ostream& operator<<(Ostream& os, const Except& e);
