// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "build.h"

#ifdef NDEBUG
#define BUILD "Release"
#else
#define BUILD "Debug"
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

const char* build = __DATE__ " " __TIME__ " (" TOSTRING(COMPILER) " " BUILD ")";
