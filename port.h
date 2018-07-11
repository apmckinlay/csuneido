#pragma once
// Copyright (c) 2003 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

void* mem_committed(int n);
void* mem_uncommitted(int size);
void mem_commit(void* p, int n);
void mem_decommit(void* p, int n);
void mem_release(void* p);

class Mmfile;
void sync_timer(Mmfile* mmf);

int fork_rebuild();

void get_exe_path(char* buf, int buflen);
