#pragma once
// Copyright (c) 2017 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "value.h"

class BuiltinArgs;

template <class T>
using MemFun = Value (T::*)(BuiltinArgs&);

template <class T>
struct Meth
	{
	Value name; // char* => symbol
	MemFun<T> fn;
	};
