// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "sockets.h"
#include "win.h"
#include "except.h"
#include "fibers.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "fatal.h"
#include "gcstring.h"
#include <ctime>
#include <unordered_map>
#include <algorithm>
using std::min;

int socket_count = 0;

//#define FILE_LOGGING

#ifdef FILE_LOGGING
#include "qpc.h"
static int t() {
	static int64_t base = qpc();
	return (qpc() - base) * 1000 / qpf;
}
#include "ostreamfile.h"
extern bool is_server;
static OstreamFile& log() {
	static OstreamFile log(is_server ? "server.log" : "client.log");
	return log;
}
#define LOG(stuff) (log() << t() << ' ' << stuff << endl), log().flush()
#elif CONSOLE_LOGGING
#include "ostreamcon.h"
#define LOG(stuff) (con() << t() << ' ' << stuff << endl)
#elif CIRC_LOGGING
#include "circlog.h"
#define LOG(stuff) CIRCLOG(t() << ' ' << stuff)
#else
#define LOG(stuff)
#endif

#define cantConnect() except("can't connect to " << addr << ":" << port)

void startup() {
	WSADATA wsadata;
	verify(0 == WSAStartup(MAKEWORD(2, 0), &wsadata));
}

void end(int sock) {
	shutdown(sock, SD_SEND);
	closesocket(sock);
}

// SocketConnect --------------------------------------------------------------

void SocketConnect::write(const char* s) {
	write(s, strlen(s));
}

void SocketConnect::write(const gcstring& s) {
	write(s.ptr(), s.size());
}

void SocketConnect::writebuf(const char* s) {
	writebuf(s, strlen(s));
}

void SocketConnect::writebuf(const gcstring& s) {
	writebuf(s.ptr(), s.size());
}

// socket server --------------------------------------------------------------

static DWORD WINAPI acceptor(LPVOID lpParameter);

struct ServerData {
	ServerData(int s, NewServerConnection ns, void* a)
		: sock(s), newServerConn(ns), arg(a),
		  mainThread(currentThreadHandle()) {
	}
	static HANDLE currentThreadHandle() {
		DWORD id = GetCurrentThreadId();
		HANDLE h = OpenThread(THREAD_SET_CONTEXT, false, id);
		verify(h != NULL);
		return h;
	}
	int sock;
	NewServerConnection newServerConn;
	void* arg;
	HANDLE mainThread;
};

// maps from port to ServerData
static std::unordered_map<int, ServerData*> serverData;

/// Creates an acceptor thread and returns
void socketServer(const char* title, int port,
	NewServerConnection newServerConn, void* arg, bool exit) {
	startup();
	LOG("socketServer port " << port);
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	verify(sock > 0);

	SOCKADDR_IN addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (0 != bind(sock, reinterpret_cast<SOCKADDR*>(&addr), sizeof addr))
		except("socket server can't bind to port " << port << " ("
												   << WSAGetLastError() << ")");
	verify(0 == listen(sock, 10));
	LOG("listening socket " << sock);

	serverData[port] = new ServerData(sock, newServerConn, arg);
	verify(CreateThread(nullptr, 4096, acceptor,
		static_cast<void*>(serverData[port]), 0, nullptr));
	++socket_count;
}

static void CALLBACK finishAccept(ULONG_PTR p);

/// Runs in a separate thread (per socket server)
/// queues finishAccept calls on the original thread
// NOTE: should do as little as possible, especially no allocation
static DWORD WINAPI acceptor(LPVOID p) {
	int errorCount = 0;
	ServerData& data = *reinterpret_cast<ServerData*>(p);
	while (true) {
		SOCKET sock = accept(data.sock, nullptr, nullptr);
		if (sock == INVALID_SOCKET) {
			if (++errorCount < 100)
				continue; // ignore

			return 0; // end thread
		}
		QueueUserAPC(
			finishAccept, data.mainThread, static_cast<ULONG_PTR>(sock));
	}
}

static int getport(int sock);
static SocketConnect* newSocketConnectAsync(int sock, void* arg);

static void CALLBACK finishAccept(ULONG_PTR p) {
	SOCKET sock = static_cast<SOCKET>(p);
	LOG("finishAccept sock " << sock << " port " << getport(sock));
	ServerData& data = *serverData[getport(sock)];
	SocketConnect* sc = newSocketConnectAsync(sock, data.arg);
	if (sc)
		Fibers::create(data.newServerConn, sc);
	else {
		LOG("newSocketConnectAsync failed");
	}
}

static int getport(int sock) {
	SOCKADDR_IN sa;
	int len = sizeof sa;
	getsockname(sock, reinterpret_cast<SOCKADDR*>(&sa), &len);
	return ntohs(sa.sin_port);
}

//-----------------------------------------------------------------------------

static struct addrinfo* resolve(const char* addr, int port) {
	struct addrinfo hints {};
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
		except("socket open failed could not resolve "
			<< addr << ":" << portstr << " " << gai_strerror(status));
	return ai;
}

struct AiFree {
	AiFree(struct addrinfo* a) : ai(a) {
	}
	struct addrinfo* operator->() const {
		return ai;
	}
	operator addrinfo*() const {
		return ai;
	}
	~AiFree() {
		freeaddrinfo(ai);
	}
	struct addrinfo* ai;
};

static int makeSocket(struct addrinfo* ai) {
	int sock = socket(ai->ai_family, SOCK_STREAM, IPPROTO_TCP);
	verify(sock > 0);
	// set TCP_NODELAY to disable Nagle
	BOOL t = true;
	verify(
		0 == setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*) &t, sizeof t));
	return sock;
}

struct SocketCloser {
	SocketCloser(int sock) : socket(sock) {
	}
	void disable() {
		socket = 0;
	}
	~SocketCloser() {
		if (socket)
			closesocket(socket);
	}
	int socket;
};

// SocketConnectSync
// ------------------------------------------------------------

class SocketConnectSync : public SocketConnect {
public:
	explicit SocketConnectSync(int s, int timeout) : sock(s) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
	}
	void write(const char* buf, int n) override;
	int read(char* dst, int required, int bufsize) override;
	bool readline(char* dst, int n) override;
	void close() override;

private:
	int sock;
	struct timeval tv {};
};

#define FDS(fds, sock) \
	struct fd_set fds {}; \
	FD_SET(sock, &(fds));

SocketConnect* socketClientSync(
	const char* addr, int port, int timeout, int timeoutConnect) {
	startup();

	AiFree ai(resolve(addr, port));

	int sock = makeSocket(ai);
	SocketCloser closer(sock);

	if (timeoutConnect == 0) { // default timeout
		if (0 != connect(sock, ai->ai_addr, ai->ai_addrlen))
			cantConnect();
	} else {
		ULONG NonBlocking = 1;
		ioctlsocket(sock, FIONBIO, &NonBlocking);
		int ret = connect(sock, ai->ai_addr, ai->ai_addrlen);
		if (ret != 0 &&
			!(ret == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK))
			cantConnect();
		FDS(wfds, sock);
		FDS(efds, sock);
		struct timeval tv; // NOLINT
		tv.tv_sec = timeoutConnect / 1000;
		tv.tv_usec = (timeoutConnect % 1000) * 1000; // ms => us
		if (0 == select(1, nullptr, &wfds, &efds, &tv))
			except("socket connection timeout");
		if (FD_ISSET(sock, &efds))
			except("socket connection error");
		ULONG Blocking = 0;
		ioctlsocket(sock, FIONBIO, &Blocking);
	}
	closer.disable();
	++socket_count;
	return new SocketConnectSync(sock, timeout);
}

void SocketConnectSync::write(const char* buf, int n) {
	if (n == 0 && wrbuf.size() == 0)
		return; // nothing to write
	WSABUF bufs[2];
	bufs[0].buf = wrbuf.buffer();
	bufs[0].len = wrbuf.size();
	wrbuf.clear();
	bufs[1].buf = const_cast<char*>(buf);
	bufs[1].len = n;

	FDS(fds, sock);
	if (0 == select(1, nullptr, &fds, nullptr, &tv))
		except("socket Write timeout");

	DWORD nSent;
	if (0 != WSASend(sock, bufs, 2, &nSent, 0, 0, 0))
		except("socket Write failed (" << WSAGetLastError() << ")");
}

int SocketConnectSync::read(char* dst, int required, int bufsize) {
	int nread = min(required, rdbuf.size());
	if (nread) {
		memcpy(dst, rdbuf.buffer(), nread);
		rdbuf.remove(nread);
	}

	verify(required <= bufsize);
	FDS(fds, sock);
	while (nread < required) {
		// NOTE: this makes timeout on each recv
		// so the entire read could exceed the timeout
		if (0 == select(1, &fds, nullptr, nullptr, &tv))
			break; // timeout

		int n = recv(sock, dst + nread, bufsize - nread, 0);
		if (n == 0)
			break; // connection closed
		if (n < 0)
			except("socket Read failed (" << WSAGetLastError() << ")");
		nread += n;
	}
	return nread;
}

// returns whether or not it got a newline
bool SocketConnectSync::readline(char* dst, int n) {
	verify(n > 1);
	*dst = 0;
	--n; // allow for nul
	char* eol;
	FDS(fds, sock);
	while (
		nullptr == (eol = (char*) memchr(rdbuf.buffer(), '\n', rdbuf.size()))) {
		if (0 == select(1, &fds, nullptr, nullptr, &tv))
			break; // timeout

		const int MINRECVSIZE = 1024;
		char* buf = rdbuf.ensure(MINRECVSIZE);
		int nr = recv(sock, buf, rdbuf.available(), 0);
		if (nr == 0)
			break; // connection closed
		if (nr < 0)
			except("socket Readline failed (" << WSAGetLastError() << ")");
		rdbuf.added(nr);
	}
	char* buf = rdbuf.buffer();
	int len = eol ? eol - buf + 1 : rdbuf.size();
	if (len < n)
		n = len;
	memcpy(dst, buf, n);
	dst[n] = 0;
	rdbuf.remove(len);
	return eol != nullptr;
}

void SocketConnectSync::close() {
	end(sock);
	--socket_count;
}

// async via overlapped
// ----------------------------------------------------------

class SocketConnectAsync;

struct Over {
	Over(SocketConnectAsync* sc_) : wsaover({}), sc(sc_), size(0) {
	}
	WSAOVERLAPPED wsaover;
	SocketConnectAsync* sc;
	int size;
};

class SocketConnectAsync : public SocketConnect {
public:
	explicit SocketConnectAsync(int s, int to = 0)
		: sock(s), wrover(this), rdover(this) {
	}
	explicit SocketConnectAsync(int s, void* arg_, const char* adr_)
		: sock(s), wrover(this), rdover(this), arg(arg_), adr(dupstr(adr_)) {
	}
	void write(const char* buf, int n) override;
	int read(char* dst, int required, int dstsize) override;
	bool readline(char* dst, int n) override;
	void close() override;
	const char* getadr() override {
		return adr;
	}
	void* getarg() override {
		return arg;
	}
	bool recv();
	void block();
	void unblock();
	WSABUF buf{};
	int required = 0; // for read
private:
	int read2(char* dst, int required, int dstsize);

	int sock;
	Over wrover;
	Over rdover;
	int blocked = -1; // fiber
	void* arg = nullptr;
	const char* adr = "";
};

SocketConnect* socketClientAsync(
	const char* addr, int port, int timeout, int timeoutConnect) {
	startup();
	LOG("async connect");

	AiFree ai(resolve(addr, port));

	int sock = makeSocket(ai);
	SocketCloser closer(sock);

	ULONG NonBlocking = 1;
	if (0 != ioctlsocket(sock, FIONBIO, &NonBlocking))
		cantConnect();

	int ret = connect(sock, ai->ai_addr, ai->ai_addrlen);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
		cantConnect();

	if (timeoutConnect <= 0)
		timeoutConnect = 60 * 1000; // default of 60 seconds
	clock_t deadline = clock() + (CLOCKS_PER_SEC * timeoutConnect) / 1000;
	struct timeval tv = {0, 5000}; // initial 5ms wait
	while (true) {
		FDS(wfds, sock);
		FDS(efds, sock);
		int result = select(1, nullptr, &wfds, &efds, &tv);
		if (SOCKET_ERROR == result)
			cantConnect();
		if (FD_ISSET(sock, &efds))
			except("socket connection error");
		if (FD_ISSET(sock, &wfds))
			break;
		if (result != 0)
			except("socket connection error");
		if (clock() > deadline)
			except("socket connection timeout");
		Fibers::sleep();
		tv = {}; // switch to 0 wait
	}

	closer.disable();
	LOG("async connected");
	++socket_count;
	return new SocketConnectAsync(sock, timeout);
}

/// completion routine
static void CALLBACK afterSend(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED wsaover, DWORD InFlags) {
	Over* over = reinterpret_cast<Over*>(wsaover);
	if (Error != 0 || BytesTransferred != over->size) { // error or closed
		LOG("async afterSend error " << Error << " BytesTransferred "
									 << BytesTransferred);
		over->sc->close();
		over->sc->unblock();
		return;
	}
	LOG("async afterSend size " << over->size << " got " << BytesTransferred);
	if (over->size != BytesTransferred)
		fatal("partial send not expected");
	over->sc->wrbuf.clear();
	over->sc->unblock();
}

void SocketConnectAsync::write(const char* src, int n) {
	int toSend = n + wrbuf.size();
	if (toSend == 0)
		return; // nothing to write
	LOG("async write buf " << wrbuf.size() << " + n " << n << " = "
						   << (wrbuf.size() + n));
	if (sock == 0)
		except("SocketConnectAsync Write socket closed");
	const int NBUFS = 2;
	WSABUF bufs[NBUFS];
	bufs[0].buf = wrbuf.buffer();
	bufs[0].len = wrbuf.size();
	bufs[1].buf = const_cast<char*>(src);
	bufs[1].len = n;

	wrover.size = toSend;
	DWORD nSent;
	int rc = WSASend(sock, bufs, NBUFS, &nSent, 0,
		reinterpret_cast<WSAOVERLAPPED*>(&wrover), afterSend);
	int err;
	if (rc == 0) {
		verify(nSent == toSend);
		SleepEx(0, true); // run afterSend
	} else if (WSA_IO_PENDING != (err = WSAGetLastError()))
		except("SocketConnectAsync Write failed (" << err << ")");
	else
		block();
	LOG("async write returning");
	// need to block rather than fire-and-forget
	// because wrover can't be used by multiple writes at once
}

int SocketConnectAsync::read(char* dst, int reqd, const int dstsize) {
	LOG("async read " << required << " up to " << dstsize);
	verify(reqd <= dstsize);
	int have = min(reqd, rdbuf.size());
	if (have) {
		memcpy(dst, rdbuf.buffer(), have);
		rdbuf.remove(have);
		reqd -= have;
	}
	if (reqd == 0)
		return have; // got data from buffer
	int result = have + read2(dst + have, reqd, dstsize - have);
	LOG("async read returning " << result << endl);
	return result;
}

// used by read and readline
int SocketConnectAsync::read2(char* dst, int reqd, const int dstsize) {
	LOG("async read2 " << required << " up to " << dstsize);
	buf.buf = dst;
	buf.len = dstsize;
	this->required = reqd;
	if (recv())
		SleepEx(0, true); // run afterRecv
	else {
		// try to avoid blocking
		// not SleepEx(1 because min sleep is about 16ms
		SleepEx(0, true);
		if (this->required > 0)
			block();
	}
	LOG("async read2 returning " << (dstsize - buf.len));
	return dstsize - buf.len;
}

/// completion routine
static void CALLBACK afterRecv(DWORD Error, DWORD BytesTransferred,
	LPWSAOVERLAPPED wsaover, DWORD InFlags) {
	Over* over = reinterpret_cast<Over*>(wsaover);
	if (Error != 0 || BytesTransferred == 0) { // error or closed
		LOG("async afterRecv error " << Error << " BytesTransferred "
									 << BytesTransferred);
		over->sc->close();
		over->sc->unblock();
		return;
	}
	LOG("async afterRecv required " << over->sc->required << " buf.len "
									<< over->sc->buf.len << " got "
									<< BytesTransferred);
	over->sc->buf.len -= BytesTransferred;
	over->sc->buf.buf += BytesTransferred;
	over->sc->required -= BytesTransferred;
	if (over->sc->required > 0)
		if (!over->sc->recv())
			return;
	over->sc->unblock();
}

// returns true if complete (or socket closed), false if pending
bool SocketConnectAsync::recv() {
	LOG("async recv buf.len " << buf.len);
	if (sock == 0)
		return true; // socket closed
	DWORD nRcvd = 0, flags = 0;
	int rc = WSARecv(sock, &buf, 1, &nRcvd, &flags,
		reinterpret_cast<WSAOVERLAPPED*>(&rdover), afterRecv);
	int err;
	if (rc == 0) {
		LOG("async WSARecv got " << nRcvd);
		return nRcvd == 0 ||   // closed
			nRcvd >= required; // complete
							   // NOTE: afterRecv will still be called
	} else if (WSA_IO_PENDING != (err = WSAGetLastError()))
		except("SocketConnectAsync Read failed (" << err << ")");
	else {
		LOG("async recv pending");
		return false; // not complete
	}
}

// returns whether or not it got a newline
bool SocketConnectAsync::readline(char* dst, int n) {
	verify(n > 1);
	*dst = 0;
	--n; // allow for nul
	char* eol;
	while (
		nullptr == (eol = (char*) memchr(rdbuf.buffer(), '\n', rdbuf.size()))) {
		const int UPTO = 1024;
		int nr = read2(rdbuf.ensure(UPTO), 1, UPTO);
		if (nr == 0)
			break; // connection closed
		rdbuf.added(nr);
	}
	char* rb = rdbuf.buffer();
	int len = eol ? eol - rb + 1 : rdbuf.size();
	if (len < n)
		n = len;
	memcpy(dst, rb, n);
	dst[n] = 0;
	rdbuf.remove(len);
	return eol != nullptr;
}

void SocketConnectAsync::block() {
	LOG("async block");
	blocked = Fibers::curFiberIndex();
	Fibers::block();
}

void SocketConnectAsync::unblock() {
	if (blocked < 0)
		return;
	LOG("async unblock");
	Fibers::unblock(blocked);
	blocked = -1;
}

void SocketConnectAsync::close() {
	LOG("async close");
	SleepEx(0, true); // run any pending completion routines
	end(sock);
	sock = 0;
	LOG("async closed" << endl);
	--socket_count;
}

static const char* getadr(SOCKET sock);

// used by socket server
static SocketConnect* newSocketConnectAsync(int sock, void* arg) {
	ULONG NonBlocking = 1;
	if (0 != ioctlsocket(sock, FIONBIO, &NonBlocking))
		return nullptr;
	return new SocketConnectAsync(sock, arg, getadr(sock));
}

static const char* getadr(SOCKET sock) {
	SOCKADDR_IN sa = {};
	int sa_len = sizeof sa;
	getpeername(sock, reinterpret_cast<sockaddr*>(&sa), &sa_len);
	auto adr = "";
	if (char* s = inet_ntoa(sa.sin_addr))
		adr = dupstr(s);
	return adr;
}
