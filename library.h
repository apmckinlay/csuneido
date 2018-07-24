#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

template <class T>
class Lisp;
class gcstring;

Lisp<gcstring> libraries();
void libload(int gnum);
Lisp<gcstring> libgetall(const char* name);
