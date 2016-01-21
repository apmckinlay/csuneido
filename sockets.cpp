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
#ifndef WINVER
#define WINVER 0x600 // needed for mingw for getaddrinfo (0x600 = Vista/2008)
#endif
#include "win.h"
#include "except.h"
#include "fibers.h"
#include "minmax.h"
#include <list>
#include "resource.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "fatal.h"

//#define LOGGING

#ifdef LOGGING
//#include "ostreamfile.h"
//static OstreamFile& log()
//	{
//	extern bool is_server;
//	static OstreamFile log(is_server ? "server.log" : "client.log");
//	return log;
//	}
//#define LOG(stuff) (log() << stuff << endl), log().flush()
#include "ostreamcon.h"
#define LOG(stuff) (con() << stuff << endl)
#include "gcstring.h"
#else
#define LOG(stuff)
#endif

const int WM_SOCKET	= WM_USER + 1;

// protect from garbage collection ==================================

template <class T, int N> class Protector
	{
public:
	void* protect(T p)
		{
		for (int i = 0; i < N; ++i)
			if (refs[i] == 0)
				return refs[i] = p;
		fatal("protect overflow");
		}
	void release(T p)
		{
		for (int i = 0; i < N; ++i)
			if (refs[i] == p)
				{ refs[i] = 0; return ; }
		error("release failed");
		}
	bool contains(T p) const
		{
		for (int i = 0; i < N; ++i)
			if (refs[i] == p)
				return true;
		return false;
		}
	int count() const
		{
		int n = 0;
		for (int i = 0; i < N; ++i)
			if (refs[i] != 0)
				++n;
		return n;
		}
//private:
	const int size = N;
	T refs[N];
	};

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

static Protector<Ldata*, 100> protldata;

static char* wndClass = socketRegisterClass();

void socketServer(char* title, int port, pNewServer newserver, void* arg, bool exit)
	{
	LOG("socketServer " << title << " " << port);
	WSADATA wsadata;
	verify(0 == WSAStartup(MAKEWORD(2,0), &wsadata));
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	verify(sock > 0);

	SOCKADDR_IN addr;
	memset(&addr, 0, sizeof addr);
	addr.sin_family =  AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (0 != bind(sock, (LPSOCKADDR) &addr, sizeof addr))
		except("can't bind to port " << port << " (" << WSAGetLastError() << ")");
	verify(0 == listen(sock, 5));

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
	protldata.protect(data);
	SetWindowLong(hwnd, GWL_USERDATA, (int) data);

	verify(0 == WSAAsyncSelect(sock, hwnd, WM_SOCKET, FD_ACCEPT));
	}

class SocketConnectAsynch;
static SocketConnectAsynch* newSocketConnectAsynch(char* title, int sock, void* arg, char* adr);
void closeChildConnections(void* arg);

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
		SocketConnectAsynch* sc = newSocketConnectAsynch(title, sock, data->arg, adr);
		Fibers::create(data->newserver, sc);
		}
	else if (msg == WM_DESTROY)
		{
		closesocket(data->sock);
		WSACleanup();
		if (data->exit)
			PostQuitMessage(0);
		closeChildConnections(data->arg);
		protldata.release(data);
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
	SocketConnectAsynch(HWND h, int s, void* a, char* n) 
		: hwnd(h), sock(s), arg(a), close_pending(false), 
		mode(CONNECT), blocked(0), adr(dupstr(n)), connect_error(false)
		{ }
	bool connect(struct addrinfo* ai);
	void event(int msg);
	void write(char* s, int n);
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
	void block()
		{
		blocked = Fibers::current();
		Fibers::block();
		}
	void unblock()
		{
		if (! blocked)
			return ;
		Fibers::unblock(blocked);
		blocked = 0;
		}
	void close2();
	void doread(char* dst, int n);
	
	HWND hwnd;
	int sock;
	void* arg;
	bool close_pending;
	enum { CONNECT, SIZE, LINE, CLOSED } mode;
	void* blocked;
	int blocked_len;
	char* adr;
	bool connect_error;
	};

static Protector<SocketConnectAsynch*, 200> protsca;

void closeChildConnections(void* arg)
	{
	for (auto sca : protsca.refs)
		if (sca && sca->getarg() == arg)
			sca->close();
	}

bool SocketConnectAsynch::connect(struct addrinfo* ai)
	{
	::connect(sock, ai->ai_addr, ai->ai_addrlen);
	block();
	return ! connect_error;
	}

void SocketConnectAsynch::write(char* s, int n)
	{
	if (mode == CLOSED)
		except("socket Write failed (connection closed)");
	LOG("write");
	wrbuf.add(s, n);
	if (wrbuf.size() > 0)
		PostMessage(hwnd, WM_SOCKET, sock, FD_WRITE);
	}

void SocketConnectAsynch::close()
	{
	LOG("close");
	if (close_pending || mode == CLOSED)
		return ;
	close_pending = true; 
	PostMessage(hwnd, WM_SOCKET, sock, FD_WRITE);
	}

static int CALLBACK connectWndProc(HWND hwnd, int msg, int wParam, int lParam);

int socketConnectionCount()
	{
	return protsca.count();
	}

static SocketConnectAsynch* newSocketConnectAsynch(char* title, int sock, void* arg, char* adr)
	{
	LOG("socketConnectAsync " << (title ? title : ""));
	HWND hwnd = CreateWindow(wndClass, title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 150, 30,
		0, 0, 0, 0);
	verify(hwnd);
	SetWindowLong(hwnd, GWL_WNDPROC, (int) connectWndProc);

	SocketConnectAsynch* sc = new SocketConnectAsynch(hwnd, sock, arg, adr);
	protsca.protect(sc);
	SetWindowLong(hwnd, GWL_USERDATA, (long) sc);
	verify(0 == WSAAsyncSelect(sock, hwnd, WM_SOCKET, 
		FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE));
	return sc;
	}

static int CALLBACK connectWndProc(HWND hwnd, int msg, int wParam, int lParam)
	{
	if (msg != WM_SOCKET)
		return DefWindowProc(hwnd, msg, wParam, lParam);

	SocketConnectAsynch* sc = (SocketConnectAsynch*) GetWindowLong(hwnd, GWL_USERDATA);
	if (sc && protsca.contains(sc))
		sc->event(lParam);
	return 0;
	}

void SocketConnectAsynch::event(int lParam)
	{
	switch (WSAGETSELECTEVENT(lParam))
		{
	case FD_CONNECT :
		if(WSAGETSELECTERROR(lParam))
			connect_error = true;
		if (blocked)
			{
			Fibers::unblock(blocked);
			blocked = 0;
			}
	case FD_READ :
		{
		LOG("FD_READ");
		const int MINRECVSIZE = 1024;
		int n;
		for (;;)
			{
			rdbuf.ensure(MINRECVSIZE);
			// ensure grows by doubling so available likely > MINRECVSIZE
			n = recv(sock, rdbuf.bufnext(), rdbuf.available(), 0);
			if (n <= 0)
				break ;
			rdbuf.added(n);
			LOG("FD_READ recv " << n);
			}
		bool closed = (n == 0);
		if (blocked && 
			(closed ||
				((mode == SIZE)
					? rdbuf.size() >= blocked_len 
					: nullptr != memchr(rdbuf.buffer(), '\n', rdbuf.size()))))
			{
			LOG("FD_READ unblocking " << (closed ? "closed" : "can read"));
			unblock();
			}
		break ;
		}
	case FD_WRITE :
		{
		int n = wrbuf.size();
		LOG("FD_WRITE wrbuf.size " << n);
		if (n > 0)
			{
			n = send(sock, wrbuf.buffer(), n, 0);
			if (n > 0)
				wrbuf.remove(n);
			}
		// once we're finished writing we can close
		else if (close_pending)
			close2();
		break ;
		}
	case FD_CLOSE :
		LOG("FD_CLOSE");
		close2();
		break ;
		}
	}

void SocketConnectAsynch::close2()
	{
	LOG("close2");
	closesocket(sock);
	DestroyWindow(hwnd);
	protsca.release(this);
	mode = CLOSED;
	unblock();
	}

int SocketConnectAsynch::read(char* dst, int n)
	{
	verify(n > 0);
	LOG("read " << n);
	if (mode == CLOSED && rdbuf.size() > 0 && n > rdbuf.size())
		n = rdbuf.size();
	if (tryread(dst, n))
		return n;
	if (mode == CLOSED)
		return 0;

	LOG("read blocking");
	mode = SIZE;
	blocked_len = n;
	block();
	if (rdbuf.size() == 0)
		return 0;
	if (rdbuf.size() < n)
		// socket has been closed, final partial read
		n = rdbuf.size();
	doread(dst, n);
	return n;
	}

bool SocketConnectAsynch::tryread(char* dst, int n)
	{
	LOG("tryread " << n << " rdbuf.size " << rdbuf.size());
	if (rdbuf.size() < n)
		return false;
	doread(dst, n);
	LOG("returning " << n);
	return true;
	}

void SocketConnectAsynch::doread(char* dst, int n)
	{
	verify(n <= rdbuf.size());
	memcpy(dst, rdbuf.buffer(), n);
	rdbuf.remove(n);
	}

bool SocketConnectAsynch::readline(char* dst, int n)
	{
	verify(n > 0);
	LOG("readline");
	*dst = 0;
	if (tryreadline(dst, n))
		return true;
	if (mode == CLOSED)
		return false;

	LOG("readline blocking");
	mode = LINE;
	block();
	// either we got a newline or the socket was closed
	LOG("readline unblocked with " << rdbuf.size());
	if (rdbuf.size() == 0)
		return false;
	if (tryreadline(dst, n))
		return true;
	// no newline so socket was closed
	--n; // allow for nul
	if (rdbuf.size() < n)
		n = rdbuf.size();
	memcpy(dst, rdbuf.buffer(), n);
	dst[n] = 0;
	rdbuf.clear(); // remove any excess > n
	return true;
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
	int len = eol - buf + 1; // includes newline
	if (len < n)
		n = len;
	memcpy(dst, buf, n);
	dst[n] = 0;
	rdbuf.remove(len);
	return true;
	}

// asynch client ====================================================

static struct addrinfo* resolve(char* addr, int port)
	{
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	// specify IPv4 since allowing IPv6 with AF_UNSPEC seems to cause problems
	// e.g. addr of "" doesn't work and it's giving IPv6 
	// although 127.0.0.1 gives IPv4
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char portstr[16];
	_itoa(port, portstr, 10);
	int status;
	struct addrinfo* ai;
	if (0 != (status = getaddrinfo(addr, portstr, &hints, &ai)))
		except("could not resolve " << addr << ":" << portstr << " " << gai_strerror(status));
	return ai;
	}

struct AiFree
	{
	AiFree(struct addrinfo* a) : ai(a)
		{ }
	struct addrinfo* operator->()
		{ return ai; }
	operator addrinfo*()
		{ return ai; }
	~AiFree()
		{
		freeaddrinfo(ai);
		}
	struct addrinfo *ai;
	};

int makeSocket(struct addrinfo* ai)
	{
	int sock = socket(ai->ai_family, SOCK_STREAM, IPPROTO_TCP);
	verify(sock > 0);
	BOOL t = true;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&t, sizeof t);
	return sock;
	}

struct SocketCloser
	{
	SocketCloser(int sock) : socket(sock)
		{ }
	void disable()
		{ socket = 0; }
	~SocketCloser()
		{ if (socket) closesocket(socket); }
	int socket;
	};

SocketConnect* socketClientAsynch(char* addr, int port)
	{
	WSADATA wsadata;
	verify(0 == WSAStartup(MAKEWORD(2,0), &wsadata));

	AiFree ai(resolve(addr, port));

	int sock = makeSocket(ai);
	SocketCloser closer(sock);

	SocketConnectAsynch* sc = newSocketConnectAsynch("", sock, 0, "");
	closer.disable();
	if (! sc->connect(ai))
		{
		protsca.release(sc);
		except("can't connect to " << addr << ":" << port);
		}
	return sc;
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

#define FDS(fds, sock) struct fd_set fds; FD_ZERO(&fds); FD_SET(sock, &fds);

SocketConnect* socketClientSynch(char* addr, int port, int timeout, int timeoutConnect)
	{
	WSADATA wsadata;
	verify(0 == WSAStartup(MAKEWORD(2,0), &wsadata));

	AiFree ai(resolve(addr, port));

	int sock = makeSocket(ai);
	SocketCloser closer(sock);

	if (timeoutConnect == 0) // default timeout
		{
		if (0 != connect(sock, ai->ai_addr, ai->ai_addrlen))
			except("can't connect to " << addr << ":" << port);
		}
	else
		{
		ULONG NonBlocking = 1;
		ioctlsocket(sock, FIONBIO, &NonBlocking);
		int ret = connect(sock, ai->ai_addr, ai->ai_addrlen);
		if (ret != 0 && ret != SOCKET_ERROR)
			except("can't connect to " << addr << ":" << port);
		FDS(wfds, sock);
		FDS(efds, sock);
		struct timeval tv;
		tv.tv_sec = timeoutConnect / 1000;
		tv.tv_usec = (timeoutConnect % 1000) * 1000; // ms => us
		if (0 == select(0, NULL, &wfds, &efds, &tv))
			except("socket connection timeout\n");
		if (FD_ISSET(sock, &efds))
			except("socket connection error\n");
		ULONG Blocking = 0;
		ioctlsocket(sock, FIONBIO, &Blocking);
		}
	closer.disable();
	return new SocketConnectSynch(sock, timeout);
	}

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

// returns whether or not it got a newline
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

		const int MINRECVSIZE = 1024;
		rdbuf.ensure(MINRECVSIZE);
		int nr = recv(sock, rdbuf.bufnext(), rdbuf.available(), 0);
		if (nr == 0)
			break ; // connection closed
		if (nr < 0)
			except("SocketClient read failed (" << WSAGetLastError() << ")");
		rdbuf.added(nr);
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
	shutdown(sock, SD_SEND);
	closesocket(sock);
	}

void SocketConnect::write(char* s)
	{ write(s, strlen(s)); }

void SocketConnect::writebuf(char* s)
	{ writebuf(s, strlen(s)); }
