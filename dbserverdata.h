#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "gcstring.h"

class DbmsQuery;
template <class T> class Lisp;

class DbServerData
	{
public:
	virtual ~DbServerData() = default;

	static DbServerData* create();

	virtual void add_tran(int tran) = 0;
	virtual int add_query(int tran, DbmsQuery* q) = 0;
	virtual DbmsQuery* get_query(int qn) = 0;
	virtual bool erase_query(int qn) = 0;
	virtual Lisp<int> get_trans() = 0;
	virtual void end_transaction(int tran) = 0;

	virtual int add_cursor(DbmsQuery* q) = 0;
	virtual DbmsQuery* get_cursor(int cn) = 0;
	virtual bool erase_cursor(int cn) = 0;

	virtual void abort(void (*fn)(int tran)) = 0;

	gcstring nonce;
	bool auth = false;
	};

class SuObject;

SuObject& dbserver_connections();
