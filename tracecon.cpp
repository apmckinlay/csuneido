// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "trace.h"
#include "ostreamcon.h"
#include "ostreamfile.h"

int trace_level = 0;

class OstreamTrace : public Ostream {
public:
	OstreamTrace() {
	}
	Ostream& write(const void* buf, int n) override {
		if (trace_level & TRACE_CONSOLE)
			con().write(buf, n);
		if (trace_level & TRACE_LOGFILE)
			log().write(buf, n);
		return *this;
	}
	void flush() override {
		if (trace_level & TRACE_CONSOLE)
			con().flush();
		if (trace_level & TRACE_LOGFILE)
			log().flush();
	}
	static Ostream& con() {
		static OstreamCon con;
		return con;
	}
	static Ostream& log() {
		static OstreamFile log("trace.log", "w");
		return log;
	}
};

Ostream& tout() {
	static OstreamTrace t;
	return t;
}
