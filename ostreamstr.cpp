// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostreamstr.h"
#include "gcstring.h"
#include "buffer.h"

OstreamStr::OstreamStr(int len) : buf(new Buffer(len)) {
}

Ostream& OstreamStr::write(const void* s, int n) {
	buf->add((const char*) s, n);
	return *this;
}

char* OstreamStr::str() {
	return buf->str();
}

gcstring OstreamStr::gcstr() {
	return buf->gcstr();
}

void OstreamStr::clear() {
	buf->clear();
}

int OstreamStr::size() const {
	return buf->size();
}
