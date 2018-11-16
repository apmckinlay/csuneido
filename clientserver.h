#pragma once
// Copyright (c) 2017 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

/// Commands for the binary client-server protocol

#include "value.h"

// sequence must match jSuneido
enum class Command {
	ABORT,
	ADMIN,
	AUTH,
	CHECK,
	CLOSE,
	COMMIT,
	CONNECTIONS,
	CURSOR,
	CURSORS,
	DUMP,
	ERASE,
	EXEC,
	EXPLAIN,
	FINAL,
	GET,
	GET1,
	HEADER,
	INFO,
	KEYS,
	KILL,
	LIBGET,
	LIBRARIES,
	LOAD,
	LOG,
	NONCE,
	ORDER,
	OUTPUT,
	QUERY,
	READCOUNT,
	REQUEST,
	REWIND,
	RUN,
	SESSIONID,
	SIZE,
	TIMESTAMP,
	TOKEN,
	TRANSACTION,
	TRANSACTIONS,
	UPDATE,
	WRITECOUNT
};

extern char* cmdnames[];
