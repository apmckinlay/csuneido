#pragma once
// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

// abstract base class for input streams
class Istream
	{
public:
	virtual ~Istream() = default;
	enum { EOFVAL = -1 };
	int get();
	Istream& getline(char* buf, int n);
	bool eof() const
		{ return eofbit; }
	explicit operator bool() const
		{ return !failbit; }
	void putback(char c)
		{ next = c; }
	int peek()
		{ return next = get(); }
	void clear()
		{ eofbit = failbit = false; }
	Istream& read(char* buf, int n);
	int gcount() const
		{ return gcnt; }
	virtual int tellg() = 0;
	virtual Istream& seekg(int pos) = 0;
protected:
	virtual int get_() = 0;
	virtual int read_(char* buf, int n) = 0;
private:
	bool eofbit = false;
	bool failbit = false;
	int next = -1;
	int gcnt = 0;
	};
