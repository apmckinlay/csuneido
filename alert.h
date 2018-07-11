#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostream.h"

Ostream& osalert();
void alert_();

#define alert(msg)	((osalert() << msg), alert_())
