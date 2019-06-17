// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "builtinclass.h"
#include "sustring.h"
#include "readline.h"
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <algorithm>
using std::min;

#ifdef _MSC_VER
#define FTELL64 _ftelli64
#define FSEEK64 _fseeki64
#else
#define FTELL64 ftello64
#define FSEEK64 fseeko64
#endif

int File_count = 0;

class SuFile : public SuValue {
public:
	void init(const char* filename, const char* mode);
	void close();
	static auto methods() {
		static Method<SuFile> methods[]{{"Close", &SuFile::Close},
			{"Flush", &SuFile::Flush}, {"Read", &SuFile::Read},
			{"Readline", &SuFile::Readline}, {"Seek", &SuFile::Seek},
			{"Tell", &SuFile::Tell}, {"Write", &SuFile::Write},
			{"Writeline", &SuFile::Writeline}};
		return gsl::make_span(methods);
	}

private:
	Value Read(BuiltinArgs&);
	Value Readline(BuiltinArgs&);
	Value Write(BuiltinArgs&);
	Value Writeline(BuiltinArgs&);
	Value Seek(BuiltinArgs&);
	Value Tell(BuiltinArgs&);
	Value Flush(BuiltinArgs&);
	Value Close(BuiltinArgs&);

	void ckopen(const char* action);

	const char* filename = nullptr;
	const char* mode = nullptr;
	FILE* f = nullptr;
	const char* end_of_line = nullptr;
};

Value su_file() {
	static BuiltinClass<SuFile> suFileClass(
		"(filename, mode = 'r', block = false)");
	return &suFileClass;
}

template <>
Value BuiltinClass<SuFile>::instantiate(BuiltinArgs& args) {
	args.usage("new File(filename, mode = 'r'");
	auto filename = args.getstr("filename");
	auto mode = args.getstr("mode", "r");
	args.end();
	SuFile* f = new BuiltinInstance<SuFile>();
	f->init(filename, mode);
	return f;
}

template <>
Value BuiltinClass<SuFile>::callclass(BuiltinArgs& args) {
	args.usage("File(filename, mode = 'r', block = false)");
	auto filename = args.getstr("filename");
	auto mode = args.getstr("mode", "r");
	Value block = args.getValue("block", SuFalse);
	args.end();

	SuFile* f = new BuiltinInstance<SuFile>();
	f->init(filename, mode);
	if (block == SuFalse)
		return f;
	Closer<SuFile*> closer(f);
	KEEPSP
	PUSH(f);
	return block.call(block, CALL, 1);
}

void SuFile::init(const char* fn, const char* m) {
	filename = fn;
	mode = m;
#if defined(_WIN32) && !defined(__clang__)
	_fmode = O_BINARY; // set default mode
#endif

	end_of_line = strchr(mode, 't') ? "\n" : "\r\n";

	if (*filename == 0 || nullptr == (f = fopen(filename, mode)))
		except(
			"File: can't open '" << filename << "' in mode '" << mode << "'");
	++File_count;
}

Value SuFile::Read(BuiltinArgs& args) {
	args.usage("file.Read(nbytes = all)");
	int64_t n = args.getint("nbytes", INT_MAX);
	args.end();

	ckopen("Read");
	if (feof(f))
		return SuFalse;
	int64_t pos = FTELL64(f);
	FSEEK64(f, 0, SEEK_END);
	int64_t end = FTELL64(f);
	FSEEK64(f, pos, SEEK_SET);
	n = min(n, end - pos);
	if (n <= 0)
		return SuFalse;
	char* buf = salloc(n);
	auto nr = fread(buf, 1, n, f);
	if (n != nr)
		except("File: Read: error reading from: " << filename << " (expected "
												  << n << " got " << nr << ")");
	return SuString::noalloc(buf, n);
}

// NOTE: Readline should be consistent across file, socket, and runpiped
Value SuFile::Readline(BuiltinArgs& args) {
	args.usage("file.Readline()").end();

	ckopen("Readline");
	int c;
	READLINE(EOF != (c = fgetc(f)));
}

Value SuFile::Write(BuiltinArgs& args) {
	args.usage("file.Write(string)");
	Value arg = args.getValue("string");
	gcstring s = arg.to_gcstr();
	args.end();

	ckopen("Write");
	if (s.size() != fwrite(s.ptr(), 1, s.size(), f))
		except("File: Write: error writing to: " << filename);
	return arg;
}

Value SuFile::Writeline(BuiltinArgs& args) {
	args.usage("file.Writeline(string)");
	Value arg = args.getValue("string");
	gcstring s = arg.to_gcstr();
	args.end();

	ckopen("Writeline");
	if (s.size() != fwrite(s.ptr(), 1, s.size(), f) ||
		fputs(end_of_line, f) < 0)
		except("File: Write: error writing to: " << filename);
	return arg;
}

Value SuFile::Seek(BuiltinArgs& args) {
	args.usage("file.Seek(offset, origin='set')");
	Value arg = args.getValue("offset");
	int64_t offset =
		arg.is_int() ? (int64_t) arg.integer() : arg.number()->bigint();
	gcstring origin_s = args.getgcstr("origin", "set");
	args.end();

	int origin;
	if (origin_s == "set")
		origin = SEEK_SET;
	else if (origin_s == "end")
		origin = SEEK_END;
	else if (origin_s == "cur")
		origin = SEEK_CUR;
	else
		except("file.Seek: origin must be 'set', 'end', or 'cur'");

	ckopen("Seek");
	if (FSEEK64(f, offset, origin) != 0)
		except("file.Seek failed");
	return Value();
}

Value SuFile::Tell(BuiltinArgs& args) {
	args.usage("file.Tell()").end();

	ckopen("Tell");
	int64_t offset = FTELL64(f);
	if (INT_MIN <= offset && offset <= INT_MAX)
		return (int) offset;
	else
		return SuNumber::from_int64(offset);
}

Value SuFile::Flush(BuiltinArgs& args) {
	args.usage("file.Flush()").end();

	ckopen("Flush");
	fflush(f);
	return Value();
}

Value SuFile::Close(BuiltinArgs& args) {
	args.usage("file.Close()").end();

	ckopen("Close");
	close();
	return Value();
}

void SuFile::ckopen(const char* action) {
	if (!f)
		except("File: can't " << action << " a closed file: " << filename);
}

void SuFile::close() {
	if (f)
		fclose(f);
	f = nullptr;
	--File_count;
}
