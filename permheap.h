#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// allocate "permanent" (never freed) memory
// NOT garbage collected
// should NOT contain reference to garbage collected memory

class PermanentHeap {
public:
	PermanentHeap(const char* name, int size);
	void* alloc(int size); // grows heap
	void free(void* p);    // shrinks heap
	bool contains(const void* p) const {
		return start <= p && p < next;
	}
	bool inarea(const void* p) const {
		return start <= p && p < limit;
	}
	void* begin() const {
		return start;
	}
	void* end() const {
		return next;
	}
	int size() const {
		return next - (char*) start;
	}
	int remaining() const {
		return (char*) limit - next;
	}

private:
	const char* name;
	void* start;
	char* next;
	char* commit_end;
	void* limit;
};
