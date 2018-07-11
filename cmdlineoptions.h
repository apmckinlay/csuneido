#pragma once
// Copyright (c) 2002 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

enum { NONE = 0, DUMP, LOAD, SERVER, CLIENT, REPL, COMPACT,
	CHECK, REBUILD, DBDUMP, TEST, BENCH, HELP, VERSION,
	UNINSTALL_SERVICE, AFTER_ACTIONS };

class CmdLineOptions
	{
public:
	CmdLineOptions()
		{ }
	const char* parse(const char* str);

	const char* s = nullptr;
	int action = NONE;
	const char* argstr = nullptr;
	int argint = 0;
	bool unattended = false;
	bool local_library = false;
	bool no_exception_handling = false;
	const char* install = nullptr;
	const char* service = nullptr;
	bool check_start = false;
	bool compact_exit = false;
	bool ignore_version = false;
	bool ignore_check = false;

private:
	int get_option();
	void set_action(int a);
	const char* get_word();
	void skip_white();
	static const char* strip_su(const char* file);
	};

extern CmdLineOptions cmdlineoptions;
