// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "fatal.h"
#include "unhandled.h"
#include <stdlib.h>
#include "cmdlineoptions.h"
#include <cstdint>

extern bool is_client;
extern void message(const char*, const char*, uint32_t timeout_ms);

void fatal(const char* error, const char* extra)
	{
	if (! cmdlineoptions.unattended)
		// deliberately not capitalized since usually out of our control
		message("Fatal Error", error, 10 * 1000); // 10 seconds
	fatal_log(error, extra); // assume we've lost connection
	}
