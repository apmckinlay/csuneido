#define ACE_AS_STATIC_LIBS 1
#include "ace/Event_Handler.h"
#include "ace/INET_Addr.h"
#include "ace/Reactor.h"
#include "ace/TP_Reactor.h"
#include "ace/Sock_Acceptor.h"
#include "ace/Sock_Stream.h"
#include "ace/Thread_Manager.h"

#include "sockets.h"
#include "dbserver.h"
#include "ostreamstd.h"
#include "except.h"
#include "minmax.h"

struct Proc;
class SesViews;

struct Tss
	{
	Tss() : proc(0), session_views(0), fiber_id("")
		{ }
	Proc* proc;
	SesViews* session_views;
	char* fiber_id;
	};
ACE_TSS<Tss> tss;

Proc*& tss_proc()
	{ return tss->proc; }

SesViews*& tss_session_views()
	{ return tss->session_views; }

char*& tss_fiber_id()
	{ return tss->fiber_id; }

class AceSocketConnect : public SocketConnect
	{
public:
	AceSocketConnect(ACE_SOCK_Stream* s) : sock(s)
		{ }
	virtual bool read(char* buf, int n);
	virtual bool readline(char* buf, int n);
	virtual void write(char* buf, int n)
		{
		iovec v[2];
		v[0].iov_base = wrbuf.buffer();
		v[0].iov_len = wrbuf.size();
		wrbuf.clear();
		v[1].iov_base = buf;
		v[1].iov_len = n;
		ACE_Time_Value timeout(10);
		sock->sendv_n(v, 2, &timeout); // what if this fails?
		}
	virtual void close()
		{ sock->close(); }
	virtual char* getadr()
		{
		ACE_INET_Addr addr;
		char buf[500];
		if (-1 == sock->get_remote_addr(addr) ||
			-1 == addr.addr_to_string(buf, sizeof buf))
			return "???" ;
		for (char* s = buf; *s; ++s)
			if (*s == ':')
				*s = 0; // remove :port
		return strdup(buf);
		}
private:
	ACE_SOCK_Stream* sock;
	};

bool AceSocketConnect::read(char* buf, int n)
	{
	int nread = min(n, rdbuf.size());
	if (nread > 0)
		{
		memcpy(buf, rdbuf.buffer(), nread);
		rdbuf.remove(nread);
		}
	if (nread < n)
		{
		if (0 >= sock->recv_n(buf + nread, n - nread))
			return false; // connection closed
		}
	return true;
	}

bool AceSocketConnect::readline(char* dst, int n)
	{
	verify(n > 0);
	*dst = 0;
	--n; // allow for nul
	char* eol;
	while (! (eol = (char*) memchr(rdbuf.buffer(), '\n', rdbuf.size())))
		{
		const int READSIZE = 1024;
		int n = sock->recv(rdbuf.reserve(READSIZE), READSIZE, 0);
		if (n <= 0)
			return false; // connection closed
//		if (n < 0)
//			except("SocketClient read failed");
		verify(n <= READSIZE);
		rdbuf.added(n);
		}
	char* buf = rdbuf.buffer();
	int len = eol - buf + 1;
	if (len < n)
		n = len;
	memcpy(dst, buf, n);
	dst[n] = 0;
	verify(len > n || dst[n - 1] == '\n');
	rdbuf.remove(len);
	return true;
	}

//===================================================================

class Acceptor : public ACE_Event_Handler
	{
public:
	Acceptor(ACE_Reactor* r = ACE_Reactor::instance()) : ACE_Event_Handler(r)
		{ }
	virtual int open(const ACE_INET_Addr& local_addr)
		{
		if (acceptor_.open(local_addr) == -1)
			return -1;
		return reactor()->register_handler(this, ACE_Event_Handler::ACCEPT_MASK);
		}
	virtual int handle_input(ACE_HANDLE = ACE_INVALID_HANDLE);
	virtual int handle_close(ACE_HANDLE = ACE_INVALID_HANDLE, ACE_Reactor_Mask = 0);
	virtual ACE_HANDLE get_handle() const
		{ return acceptor_.get_handle(); }
	ACE_SOCK_Acceptor& acceptor()
		{ return acceptor_; }
	typedef ACE_INET_Addr PEER_ADDR;

protected:
	virtual ~Acceptor()
		{ }
private:
	ACE_SOCK_Acceptor acceptor_;
	};

class DbServer;

class Handler : public ACE_Event_Handler
	{
public:
	Handler(ACE_Reactor* r);
	virtual int open();
	virtual int handle_input(ACE_HANDLE = ACE_INVALID_HANDLE); // new connection to accept
	virtual int handle_close(ACE_HANDLE = ACE_INVALID_HANDLE, ACE_Reactor_Mask = 0);
	virtual ACE_HANDLE get_handle() const
		{ return peer_.get_handle(); }
	ACE_SOCK_Stream& peer()
		{ return peer_; }
private:
	ACE_SOCK_Stream peer_; // how does this get set ???
	AceSocketConnect* sc;
	DbServer* dbs;
	};

int Acceptor::handle_input(ACE_HANDLE)
	{
	// new connection creates new handler
	Handler* handler;
	ACE_NEW_RETURN(handler, Handler(reactor()), -1);
	if (acceptor_.accept(handler->peer()) == -1)
		{
		delete handler;
		return -1;
		}
	else if (handler->open() == -1)
		{
		handler->handle_close();
		return -1;
		}
	return 0;
	}

int Acceptor::handle_close(ACE_HANDLE, ACE_Reactor_Mask)
	{
	acceptor_.close();
	delete this;
	return 0;
	}

class Quit_Handler : public ACE_Event_Handler
	{
public:
	Quit_Handler(ACE_Reactor* r) : ACE_Event_Handler(r)
		{ }
	virtual int handle_exception(ACE_HANDLE)
		{
		reactor()->end_reactor_event_loop();
		return -1;
		}
private:
	virtual ~Quit_Handler()
		{ }
	};

Quit_Handler* quit_handler = 0;

//===================================================================
// these are the key methods to hook into it
// these are just sample implementations

Handler::Handler(ACE_Reactor* r) : ACE_Event_Handler(r)
	{ }

int Handler::open()
	{
	sc = new AceSocketConnect(&peer_);
	dbs = DbServer::make(sc);
	return reactor()->register_handler(this, ACE_Event_Handler::READ_MASK);
	}

#include "gcstring.h"

int Handler::handle_input(ACE_HANDLE) // this is the actual server implementation
	{
	char buf[32000];
	if (! sc->readline(buf, sizeof buf))
		return -1; // socket closed
//cout << buf;
	try
		{
		dbs->request(buf);
		}
	catch (const Except& x)
		{
cout << x.exception << endl;
		}
	catch (const std::exception& e)
		{
cout << e.what() << endl;
		}
	catch (...)
		{
cout << "unknown exception" << endl;
		}
	return 0;
	}

int Handler::handle_close(ACE_HANDLE, ACE_Reactor_Mask)
	{
	delete dbs;
	delete this;
	return 0;
	}

//===================================================================

extern int su_port;

template <class ACCEPTOR> class Server : public ACCEPTOR
	{
public:
	Server(ACE_Reactor* reactor) : ACCEPTOR(reactor)
		{
		ACE_TYPENAME ACCEPTOR::PEER_ADDR server_addr;
		u_short port = su_port;
		int result = server_addr.set(port, (ACE_UINT32) INADDR_ANY);
		if (result != -1)
			result = ACCEPTOR::open(server_addr);
		if (result == -1)
			reactor->end_reactor_event_loop();
		}
	};

static ACE_THR_FUNC_RETURN event_loop(void* arg)
	{
	ACE_Reactor* reactor = ACE_static_cast(ACE_Reactor*, arg);
	reactor->owner(ACE_OS::thr_self());
	try
		{
		reactor->run_reactor_event_loop();
		}
	catch (const Except& x)
		{
cout << x.exception << endl;
		}
	catch (const std::exception& e)
		{
cout << e.what() << endl;
		}
	catch (...)
		{
cout << "unknown exception" << endl;
		}
	return 0;
	}

int aceserver(int n_threads)
	{
	ACE::init();
	ACE_TP_Reactor tp_reactor;
	ACE_Reactor reactor(&tp_reactor);
	auto_ptr<ACE_Reactor> delete_instance(ACE_Reactor::instance(&reactor));
	Server<Acceptor>* server = 0;
	ACE_NEW_RETURN(server, Server<Acceptor>(&reactor), 1);
	ACE_NEW_RETURN(quit_handler, Quit_Handler(&reactor), 1);
	ACE_Thread_Manager::instance()->spawn_n(n_threads, event_loop, ACE_Reactor::instance());
	return ACE_Thread_Manager::instance()->wait();
	}
