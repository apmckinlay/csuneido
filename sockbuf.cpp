#include "sockets.h"
#include "except.h"
#include "gc.h"
#include <string.h>

SockBuf::SockBuf(int n) : cap(n), siz(0), buf(new(noptrs) char[n])
	{ }

char* SockBuf::reserve(int n)
	{
	if (siz + n > cap)
		buf = (char*) GC_realloc(buf, cap = 2 * cap + n);
	return buf + siz;
	}

void SockBuf::add(char* s, int n)
	{
	memcpy(reserve(n), s, n);
	added(n);
	}

void SockBuf::remove(int n)
	{
	verify(n <= siz);
	memmove(buf, buf + n, siz - n);
	siz -= n;
	}

void SocketConnect::write(char* s)
	{ write(s, strlen(s)); }

void SocketConnect::writebuf(char* s)
	{ writebuf(s, strlen(s)); }
