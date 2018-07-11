// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "commalist.h"
#include "lisp.h"
#include "gcstring.h"

Lisp<gcstring> commas_to_list(const gcstring& s)
	{
	Lisp<gcstring> list;
	if (s.size() == 0)
		return list;
	int i, j;
	for (i = 0; -1 != (j = s.find(',', i)); i = j + 1)
		list.push(s.substr(i, j - i));
	list.push(s.substr(i));
	return list.reverse();
	}

gcstring list_to_commas(Lisp<gcstring> list)
	{
	gcstring s;
	for (; ! nil(list); ++list)
		{
		s += *list;
		if (! nil(cdr(list)))
			s += ",";
		}
	return s;
	}
