#pragma once
// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

class Heap
	{
public:
	explicit Heap(bool executable = false);
	void* alloc(int n);
	void free(void* p);
	void destroy();
	~Heap();
private:
	void* heap;
	};
