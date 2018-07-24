// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "alert.h"
#include "alertout.h"
#include "ostreamstr.h"
#include "cmdlineoptions.h"
#include "errlog.h"

static OstreamStr os;

Ostream& osalert() {
	return os;
}

void alert_() {
	auto msg = dupstr(os.str());
	os.clear(); // before alert to allow for nesting
	if (cmdlineoptions.unattended)
		errlog("Alert:", msg);
	else
		alertout(msg);
}
