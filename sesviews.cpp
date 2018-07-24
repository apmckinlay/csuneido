// Copyright (c) 2004 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "sesviews.h"
#include "fibers.h" // for tls()

void set_session_view(const gcstring& name, const gcstring& def) {
	if (!tls().session_views)
		tls().session_views = new SesViews;
	(*tls().session_views)[name] = def;
}

gcstring get_session_view(const gcstring& name) {
	if (tls().session_views)
		if (gcstring* p = tls().session_views->find(name))
			return *p;
	return "";
}

void remove_session_view(const gcstring& name) {
	if (tls().session_views)
		tls().session_views->erase(name);
}
