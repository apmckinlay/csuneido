#include "ace/Event_Handler.h"
#include "ace/INET_Addr.h"
#include "ace/Reactor.h"
#include "ace/TP_Reactor.h"
#include "ace/Sock_Acceptor.h"
#include "ace/Sock_Stream.h"
#include "ace/Thread_Manager.h"

#include "sockets.h"

class AceSocketConnect : public SocketConnect
	{
public:
	AceSocketConnect(ACE_SOCK_Stream* s) : sock(s)
		{ }
	virtual bool read(char* buf, int n)
		{
		sock->recv_n(buf, n);
		}
	virtual bool readline(char* buf, int n)
		{ return false; } // not used
	virtual void write(char* buf, int n)
		{
		iovec v[2];
		v[0].iov_base = wrbuf.buffer();
		v[0].iov_len = wrbuf.size();
		v[1].iov_base = buf;
		v[1].iov_len = n;
		ACE_Time_Value timeout(10);
		sock->sendv_n(v, 2, &timeout); // what if this fails?
		}
	virtual void close()
		{ }
	virtual char* getadr()
		{ return ""; } // TODO: implement
private:
	ACE_SOCK_Stream* sock;
	};

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
DbServer* make_dbserver(SocketConnect*);

class Handler : public ACE_Event_Handler
	{
public:
	Handler(ACE_Reactor* r) : ACE_Event_Handler(r), sc(&peer_), dbs(make_dbserver(&sc))
		{ }
	virtual int open();
	virtual int handle_input(ACE_HANDLE = ACE_INVALID_HANDLE); // new connection to accept
	virtual int handle_close(ACE_HANDLE = ACE_INVALID_HANDLE, ACE_Reactor_Mask = 0);
	virtual ACE_HANDLE get_handle() const
		{ return peer_.get_handle(); }
	ACE_SOCK_Stream& peer()
		{ return peer_; }
private:
	ACE_SOCK_Stream peer_; // how does this get set ???
	AceSocketConnect sc;
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

int Handler::open()
	{
	peer().send_n("hello\r\n", 7);
	return reactor()->register_handler(this, ACE_Event_Handler::READ_MASK);
	}

int Handler::handle_input(ACE_HANDLE) // this is the actual server implementation
	{
	char buf[1000];
	ssize_t n = peer().recv(buf, sizeof buf);
	if (n <= 0)
		return -1;
	if (buf[0] == 'q')
		reactor()->notify(quit_handler);
	peer().send_n(buf, n);
	return 0;
	}

int Handler::handle_close(ACE_HANDLE, ACE_Reactor_Mask)
	{
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
	reactor->run_reactor_event_loop();
	return 0;
	}

int aceserver(int n_threads)
	{
	ACE_TP_Reactor tp_reactor;
	ACE_Reactor reactor(&tp_reactor);
	auto_ptr<ACE_Reactor> delete_instance(ACE_Reactor::instance(&reactor));
	Server<Acceptor>* server = 0;
	ACE_NEW_RETURN(server, Server<Acceptor>(&reactor), 1);
	ACE_NEW_RETURN(quit_handler, Quit_Handler(&reactor), 1);
	ACE_Thread_Manager::instance()->spawn_n(n_threads, event_loop, ACE_Reactor::instance());
	return ACE_Thread_Manager::instance()->wait();
	}
