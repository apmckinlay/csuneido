#pragma once
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2000 Suneido Software Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation - version 2.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License in the file COPYING
 * for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

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
	bool auth;
	};

class SuObject;

SuObject& dbserver_connections();
