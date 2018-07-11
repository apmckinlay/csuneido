#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "gcstring.h"

class DbserverData;

class Auth
	{
public:
	enum { NONCE_SIZE = 8 };
	enum { TOKEN_SIZE = 16 };
	static gcstring token();
	static gcstring nonce();
	static bool auth(const gcstring& nonce, const gcstring& data);
	};
