// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "range.h"

int prepFrom(int from, int len) {
	if (from < 0) {
		from += len;
		if (from < 0)
			from = 0;
	}
	return from;
}

int prepTo(int to, int len) {
	if (to < 0)
		to += len;
	if (to > len)
		to = len;
	return to;
}

int prepLen(int len, int size) {
	if (len < 0)
		len = 0;
	if (len > size)
		len = size;
	return len;
}

//-------------------------------------------------------------------

#include "porttest.h"
#include "sustring.h"
#include "suobject.h"
#include "ostreamstr.h"

static SuObject* stringToCharList(gcstring s) {
	auto ob = new SuObject();
	for (int i = 0; i < s.size(); ++i)
		ob->add(new SuString(s.substr(i, 1)));
	return ob;
}

PORTTEST(lang_range) {
	SuString s(args[0]);
	int from = atoi(args[1].str());
	int to = atoi(args[2].str());
	Value expected(new SuString(args[3]));
	Value result = s.rangeTo(from, to);
	if (result != expected)
		return OSTR("got: " << result);

	SuObject* list = stringToCharList(args[0]);
	Value expectedList = stringToCharList(args[3]);
	result = list->rangeTo(from, to);
	if (result != expectedList)
		return OSTR("got: " << result);
	return nullptr;
}

const char* pt_method(const List<gcstring>& args, const List<bool>& str);

PORTTEST(lang_sub) {
	SuString s(args[0]);
	int org = atoi(args[1].str());
	int len = args.size() == 3 ? 9999 : atoi(args[2].str());
	int last = args.size() - 1;
	Value expected(new SuString(args[last]));
	Value result = s.rangeLen(org, len);
	if (result != expected)
		return OSTR("expected: " << expected << "got: " << result);

	List<gcstring> args2;
	args2.add(args[0]).add("Substr");
	for (int i = 1; i < args.size(); ++i)
		args2.add(args[i]);
	List<bool> str2;
	str2.add(str[0]).add(true);
	for (int i = 1; i < args.size(); ++i)
		str2.add(str[i]);
	if (auto err = pt_method(args2, str2))
		return OSTR("Substr " << err);

	SuObject* list = stringToCharList(args[0]);
	Value expectedList = stringToCharList(args[last]);
	result = list->rangeLen(org, len);
	if (result != expectedList)
		return OSTR("got: " << result);

	args2[0] = OSTR(list);
	str2[0] = false;
	args2[1] = "Slice";
	last = args2.size() - 1;
	args2[last] = OSTR(expectedList);
	str2[last] = false;
	if (auto err = pt_method(args2, str2))
		return OSTR("Slice " << err);

	return nullptr;
}
