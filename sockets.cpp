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

#include "sockets.h"
#include "win.h"
#include "except.h"
#include "fibers.h"
#include "minmax.h"
#include <list>
#include "resource.h"
#include <winsock2.h>
#include "gc.h"
#include "fatal.h"

//#define LOGGING

#ifdef LOGGING
#include "ostreamfile.h"
static OstreamFile& log()
	{
	extern bool is_server;
	static OstreamFile log(is_server ? "server.log" : "client.log");
	return log;
	}
#define LOG(stuff) (log() << stuff << endl), log().flush()
#include "gcstring.h"
#else
#define LOG(stuff)
#endif

const int WM_SOCKET	= WM_USER + 1;

// protect from garbage collection ==================================

class Protector
	{
public:
	void* protect(void* p);
	void release(void* p);
private:
	enum { maxrefs = 99 };
	void* refs[maxrefs];
	};

void* Protector::protect(void* p)
	{
	for (int i = 0; i < maxrefs; ++i)
		if (refs[i] == 0)
			return refs[i] = p;
	fatal("protect overflow");
	}

void Protector::release(void* p)
	{
	for (int i = 0; i < maxrefs; ++i)
		if (refs[i] == p)
			{ refs[i] = 0; return ; }
	error("release failed");
	}

static Protector protector;

// register a window class ==========================================

static char* socketRegisterClass()
	{
	WNDCLASS wc;
	memset(&wc, 0, sizeof wc);
	wc.lpszClassName = "socket";
	wc.lpfnWndProc = DefWindowProc;
	wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_SUNEIDO));
	verify(RegisterClass(&wc));
	return "socket";
	}

// listener =========================================================

static int CALLBACK listenWndProc(HWND hwnd, int msg, int wParam, int lParam);

struct Ldata
	{
	Ldata(char* t, int s, pNewServer ns, void* a, bool e) 
		: title(t), sock(s), newserver(ns), arg(a), exit(e)
		{ }
	char* title;
	int sock;
	pNewServer newserver;
	void* arg;
	bool exit;
	};

static char* wndClass = socketRegisterClass();

void socketServer(char* title, int port, pNewServer newserver, void* arg, bool exit)
	{
	LOG("socketServer " << title << " " << port);
	WSADATA wsadata;
	verify(0 == WSAStartup(MAKEWORD(2,0), &wsadata));
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	verify(sock > 0);

	RECT rect;
	GetWindowRect(GetDesktopWindow(), &rect);
	HWND hwnd = CreateWindow(wndClass, title, WS_OVERLAPPEDWINDOW,
		rect.right - 255, 5, 250, 50,
		0, 0, 0, 0);
	verify(hwnd);
	SetWindowLong(hwnd, GWL_WNDPROC, (int) listenWndProc);
	if (*title)
		{
		ShowWindow(hwnd, SW_SHOWNORMAL);
		UpdateWindow(hwnd);
		}
	
	Ldata* data = new Ldata(title, sock, newserver, arg, exit);
	protector.protect(data);
	SetWindowLong(hwnd, GWL_USERDATA, (int) data);

	// to avoid problems restarting server
	// as recommended by Effective TCP/IP Programming
	const BOOL on = TRUE;
	verify(0 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*) &on, sizeof on));

	SOCKADDR_IN addr;
	memset(&addr, 0, sizeof addr);
	addr.sin_family =  AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (0 != bind(sock, (LPSOCKADDR) &addr, sizeof addr))
		except("can't bind to port " << port << " (" << WSAGetLastError() << ")");
	verify(0 == listen(sock, 5));
	verify(0 == WSAAsyncSelect(sock, hwnd, WM_SOCKET, FD_ACCEPT));
	}

static SocketConnect* socketConnect(char* title, int sock, void* arg, char* adr);

static int CALLBACK listenWndProc(HWND hwnd, int msg, int wParam, int lParam)
	{
	Ldata* data = (Ldata*) GetWindowLong(hwnd, GWL_USERDATA);
	if (msg == WM_SOCKET && LOWORD(lParam) == FD_ACCEPT)
		{
		SOCKADDR_IN client;
		memset(&client, 0, sizeof client);
		int client_size = sizeof client;
		int sock = accept(wParam, (LPSOCKADDR) &client, &client_size);
		if (sock == INVALID_SOCKET)
			return 0;
		IN_ADDR client_in;
		memcpy(&client_in, &client.sin_addr.s_addr, sizeof client_in);
		char adr[20] = "";
		if (char* s = inet_ntoa(client_in))
			strcpy(adr, s);
		char title[512] = "";
		if (*data->title)
			{
			strcpy(title, data->title);
			strcat(title, " ");
			strcat(title, adr);
			}
		SocketConnect* sc = socketConnect(title, sock, data->arg, adr);
		Fibers::create(data->newserver, sc);
		}
	else if (msg == WM_DESTROY)
		{
		closesocket(data->sock);
		WSACleanup();
		if (data->exit)
			PostQuitMessage(0);
		protector.release(data);
		}
	else if (msg == WM_ENDSESSION)
		{
		exit(0);
		}
	else
		return DefWindowProc(hwnd, msg, wParam, lParam);
	return 0;
	}

// asynch connection - used by server & async client ================

class SocketConnectAsynch : public SocketConnect
	{
public:
	SocketConnectAsynch(HWND h, int s, void* a, char* n) : hwnd(h), sock(s), arg(a), close_pending(false), 
		mode(SIZE), blocked(0), adr(strdup(n))
		{ }
	void event(int msg);
	void write(char* s, int n);
	void writebuf(char* s, int n);
	int read(char* dst, int n);
	bool tryread(char* dst, int n);
	bool readline(char* dst, int n);
	bool tryreadline(char* dst, int n);
	void close();
	void* getarg()
		{ return arg; }
	char* getadr()
		{ return adr; }
private:
	HWND hwnd;
	int sock;
	void* arg;
	bool close_pending;
	enum { SIZE, LINE, CLOSED } mode;
	void* blocked;
	char* blocked_buf;
	int blocked_len;
	bool blocked_readok;
	char* adr;
	};

void SocketConnectAsynch::write(char* s, int n)
	{
	wrbuf.add(s, n);
	event(FD_WRITE);
	}

void SocketConnectAsynch::close()
	{
	if (close_pending)
		return ;
	close_pending = true; 
	PostMessage(hwnd, WM_SOCKET, sock, FD_WRITE);
	}

static int CALLBACK connectWndProc(HWND hwnd, int msg, int wParam, int lParam);

static SocketConnect* socketConnect(char* title, int sock, void* arg, char* adr)
	{
	LOG("socketConnect " << (title ? title : ""));
	HWND hwnd = CreateWindow(wndClass, title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 150, 30,
		0, 0, 0, 0);
	verify(hwnd);
	SetWindowLong(hwnd, GWL_WNDPROC, (int) connectWndProc);

	SocketConnect* sc = new SocketConnectAsynch(hwnd, sock, arg, adr);
	protector.protect(sc);
	SetWindowLong(hwnd, GWL_USERDATA, (long) sc);
	verify(0 == WSAAsyncSelect(sock, hwnd, WM_SOCKET, FD_READ + FD_WRITE + FD_CLOSE));
	return sc;
	}

static int CALLBACK connectWndProc(HWND hwnd, int msg, int wParam, int lParam)
	{
	if (msg != WM_SOCKET)
		return DefWindowProc(hwnd, msg, wParam, lParam);

	SocketConnectAsynch* sc = (SocketConnectAsynch*) GetWindowLong(hwnd, GWL_USERDATA);
	sc->event(LOWORD(lParam));
	return 0;
	}

void SocketConnectAsynch::event(int msg)
	{
	switch (msg)
		{
	case FD_READ :
		{
		const int READSIZE = 1024;
		int n = recv(sock, rdbuf.reserve(READSIZE), READSIZE, 0);
		if (n <= 0)
			break ;
		rdbuf.added(n);
		if (blocked && (mode == SIZE 
			? tryread(blocked_buf, blocked_len) 
			: tryreadline(blocked_buf, blocked_len)))
			{
			blocked_readok = true;
			Fibers::unblock(blocked);
			blocked = 0;
			}
		break ;
		}
	case FD_WRITE :
		{
		int n = wrbuf.size();
		if (n > 0)
			{
			n = send(sock, wrbuf.buffer(), n, 0);
			if (n > 0)
				wrbuf.remove(n);
			break ;
			}
		else if (! close_pending)
			break ;
		// else
			// fall thru
		}
	case FD_CLOSE :
		closesocket(sock);
		DestroyWindow(hwnd);
		mode = CLOSED;
		if (blocked)
			Fibers::unblock(blocked);
		protector.release(this);
		break ;
		}
	}

int SocketConnectAsynch::read(char* dst, int n)
	{
	if (tryread(dst, n))
		return n;
	if (mode == CLOSED)
		return 0;

	mode = SIZE;
	blocked_len = n;
	blocked_buf = dst;
	blocked_readok = false;
	blocked = Fibers::current();
	Fibers::block();
	return blocked_readok;
	}

bool SocketConnectAsynch::tryread(char* dst, int n)
	{
	if (rdbuf.size() < n)
		return false;

	// data available
	memcpy(dst, rdbuf.buffer(), n);
	rdbuf.remove(n);
	return true;
	}

bool SocketConnectAsynch::readline(char* dst, int n)
	{
	if (tryreadline(dst, n))
		return true;
	if (mode == CLOSED)
		return false;

	mode = LINE;
	blocked_len = n;
	blocked_buf = dst;
	blocked_readok = false;
	blocked = Fibers::current();
	Fibers::block();
	return blocked_readok;
	}

bool SocketConnectAsynch::tryreadline(char* dst, int n)
	{
	--n; // allow for nul
	char* buf = rdbuf.buffer();
	char* eol = (char*) memchr(buf, '\n', rdbuf.size());
	if (! eol)
		return false;

	// removes entire line, regardless of length
	// but only returns as many characters as requested
	int len = eol - buf + 1;
	if (len < n)
		n = len;
	memcpy(dst, buf, n);
	dst[n] = 0;
	rdbuf.remove(len);
	return true;
	}

// asynch client ====================================================

SocketConnect* socketClientAsynch(char* addr, int port)
	{
	WSADATA wsadata;
	verify(0 == WSAStartup(MAKEWORD(2,0), &wsadata));
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	verify(sock > 0);

	SOCKADDR_IN saddr;
	memset(&saddr, 0, sizeof saddr);
	saddr.sin_family =  AF_INET;
	saddr.sin_port = htons(port);
	if (INADDR_NONE == (saddr.sin_addr.s_addr = inet_addr(addr)))
		{
		struct hostent* h;
		if (! (h = gethostbyname(addr)))
			except("unknown address: " << addr);
		saddr.sin_addr.s_addr = *(u_long *) h->h_addr;
		}
	if (0 != connect(sock, (LPSOCKADDR) &saddr, sizeof saddr))
		except("can't connect to " << addr << " port " << port);
	return socketConnect("", sock, 0, "");
	}

// synch client =====================================================

class SocketConnectSynch : public SocketConnect
	{
public:
	explicit SocketConnectSynch(int s, int timeout) : sock(s)
		{
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		}
	void writebuf(char* buf, int n);
	void write(char* buf, int n);
	int read(char* dst, int n);
	bool readline(char* dst, int n);
	void close();
	char* getadr()
		{ return ""; }
private:
	int sock;
	struct timeval tv;
	};

SocketConnect* socketClientSynch(char* addr, int port, int timeout)
	{
	WSADATA wsadata;
	verify(0 == WSAStartup(MAKEWORD(2,0), &wsadata));
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	verify(sock > 0);

	SOCKADDR_IN saddr;
	memset(&saddr, 0, sizeof saddr);
	saddr.sin_family =  AF_INET;
	saddr.sin_port = htons(port);
	if (INADDR_NONE == (saddr.sin_addr.s_addr = inet_addr(addr)))
		{
		struct hostent* h;
		if (! (h = gethostbyname(addr)))
			except("unknown address: " << addr);
		saddr.sin_addr.s_addr = *(u_long *) h->h_addr;
		}
	if (0 != connect(sock, (LPSOCKADDR) &saddr, sizeof saddr))
		except("can't connect to " << addr << " port " << port);
	
	return new SocketConnectSynch(sock, timeout);
	}

#define FDS(fds, sock) struct fd_set fds; FD_ZERO(&fds); FD_SET(sock, &fds);

void SocketConnectSynch::write(char* buf, int n)
	{
	LOG(gcstring(wrbuf.size(), wrbuf.buffer()) << gcstring(n, buf) << "EOW");
	WSABUF bufs[2];
	bufs[0].buf = wrbuf.buffer();
	bufs[0].len = wrbuf.size();
	wrbuf.clear();
	bufs[1].buf = buf;
	bufs[1].len = n;

	FDS(fds, sock);
	if (0 == select(1, NULL, &fds, NULL, &tv))
		except("SocketClient write timed out");

	DWORD nSent;
	if (0 != WSASend(sock, bufs, 2, &nSent, 0, 0, 0))
		except("SocketClient write failed (" << WSAGetLastError() << ")");
	}

int SocketConnectSynch::read(char* dst, int dstsize)
	{
	int nread = min(dstsize, rdbuf.size());
	if (nread)
		{
		memcpy(dst, rdbuf.buffer(), nread);
		rdbuf.remove(nread);
		}

	FDS(fds, sock);
	while (nread < dstsize)
		{
		if (0 == select(1, &fds, NULL, NULL, &tv))
			break ; // timeout

		int n = recv(sock, dst + nread, dstsize - nread, 0);
		if (n == 0)
			break ; // connection closed
		if (n < 0)
			except("SocketClient read failed (" << WSAGetLastError() << ")");
		nread += n;
		}
	return nread;
	}

bool SocketConnectSynch::readline(char* dst, int n)
	{
	verify(n > 1);
	*dst = 0;
	--n; // allow for nul
	char* eol;
	FDS(fds, sock);
	while (! (eol = (char*) memchr(rdbuf.buffer(), '\n', rdbuf.size())))
		{
		if (0 == select(1, &fds, NULL, NULL, &tv))
			break ; // timeout

		const int READSIZE = 1024;
		int n = recv(sock, rdbuf.reserve(READSIZE), READSIZE, 0);
		if (n == 0)
			break ; // connection closed
		if (n < 0)
			except("SocketClient read failed (" << WSAGetLastError() << ")");
		verify(n <= READSIZE);
		rdbuf.added(n);
		}
	char* buf = rdbuf.buffer();
	int len = eol ? eol - buf + 1 : rdbuf.size();
	if (len < n)
		n = len;
	memcpy(dst, buf, n);
	dst[n] = 0;
	rdbuf.remove(len);
	LOG("\t=> " << dst << "EOR");
	return eol != 0;
	}

void SocketConnectSynch::close()
	{
	// as recommended by Winsock FAQ
	const int LEN = 1024;
	char buf[LEN];
	shutdown(sock, SD_SEND);
// TODO: need timeout on recv
	for (int n = 0; 0 < recv(sock, buf, LEN, 0); ++n)
		verify(n < 100);
	closesocket(sock);
	}
