// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "suboolean.h"
#include "sunumber.h"
#include "ostream.h"
#include "gcstring.h"
#include "except.h"

SuBoolean* SuBoolean::t = new SuBoolean(1);
SuBoolean* SuBoolean::f = new SuBoolean(0);

int SuBoolean::integer() const {
	if (!val)
		return 0; // false => 0
	except("can't convert true to number");
}

SuNumber* SuBoolean::number() {
	if (!val)
		return &SuNumber::zero; // false => 0
	except("can't convert true to number");
}

void SuBoolean::out(Ostream& os) const {
	os << to_gcstr();
}

gcstring SuBoolean::to_gcstr() const {
	static gcstring ts("true");
	static gcstring fs("false");
	return val ? ts : fs;
}

size_t SuBoolean::packsize() const {
	return 1;
}

void SuBoolean::pack(char* buf) const {
	*buf = val;
}

SuBoolean* SuBoolean::unpack(const gcstring& s) {
	return s.size() > 0 && s[0] ? t : f;
}

SuBoolean* SuBoolean::literal(const char* s) {
	if (0 == strcmp(s, "true"))
		return t;
	else if (0 == strcmp(s, "false"))
		return f;
	else
		return nullptr;
}

static int ord = ::order("Boolean");

int SuBoolean::order() const {
	return ord;
}

bool SuBoolean::eq(const SuValue& y) const {
	if (y.order() == ord)
		return val == static_cast<const SuBoolean&>(y).val;
	else
		return false;
}

bool SuBoolean::lt(const SuValue& y) const {
	if (y.order() == ord)
		return val < static_cast<const SuBoolean&>(y).val;
	else
		return ord < y.order();
}
