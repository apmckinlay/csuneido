// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "ostreamcon.h"
#include "win.h"

class OstreamCon : public Ostream {
public:
	OstreamCon() {
		AllocConsole();
		con = GetStdHandle(STD_OUTPUT_HANDLE);
	}
	Ostream& write(const void* buf, int n) override {
		DWORD nw;
		WriteFile(con, buf, n, &nw, NULL);
		return *this;
	}
	explicit operator bool() const {
		return con;
	}

private:
	HANDLE con;
};

Ostream& con() {
	static OstreamCon con;
	return con;
}
