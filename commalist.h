#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

class gcstring;
template <class T> class Lisp;

// convert a comma separted string to a list of strings
Lisp<gcstring> commas_to_list(const gcstring& str);

// convert a list of strings to a comma separated string
gcstring list_to_commas(Lisp<gcstring> list);
