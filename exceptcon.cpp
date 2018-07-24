// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "except.h"
#include "ostreamstr.h"
#include <stdio.h>
#include <stdlib.h>
#include "alert.h"

// dummy version for console

Ostream& operator<<(Ostream& os, const Except& x) {
	return os << x.exception;
}

static OstreamStr os;

Ostream& osexcept() {
	return os;
}

void except_() {
	alert(os.str());
	exit(EXIT_FAILURE);
}
