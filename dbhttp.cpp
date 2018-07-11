// Copyright (c) 2001 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

#include "dbhttp.h"
#include "sockets.h"
#include "fibers.h"
#include "ostreamstr.h"
#include <time.h>
#include "thedb.h"
#include "database.h"
#include "suobject.h"
#include "gc.h"
#include "build.h"

class DbHttp
	{
public:
	DbHttp(SocketConnect* s) : sc(s)
		{ }
	void run() const;
private:
	SocketConnect* sc;
	};

static void _stdcall dbhttp(void* sc)
	{
	DbHttp dbh((SocketConnect*) sc);
	dbh.run();
	Fibers::end();
	}

extern int su_port;
void start_dbhttp()
	{
	socketServer("", su_port + 1, dbhttp, 0, false);
	}

#define MB(n) ((n + 512 * 1024) / (1024*1024))

extern SuObject& dbserver_connections();
extern int tempdest_inuse;
extern int cursors_inuse;

void DbHttp::run() const
	{
	try
		{
		const int bufsize = 512;
		char buf[bufsize];
		sc->readline(buf, bufsize);

		SuObject& conns = dbserver_connections();
		conns.sort();
		OstreamStr page;
		page << "<html>\r\n"
			<< "<head>\r\n"
			<< "<title>Suneido Server Monitor</title>\r\n"
			<< "<meta http-equiv=\"refresh\" content=\"15\" />\r\n"
			<< "</head>\r\n"
			<< "<body>\r\n"
			<< "<h1>Suneido Server Monitor</h1>\r\n"
			<< "<p>Built: " << build << "</p>\r\n"
			<< "<p>Heap Size: " << MB(GC_get_heap_size()) << "mb</p>\r\n"
			<< "<p>Temp Dest: " << tempdest_inuse << "</p>\r\n"
			<< "<p>Transactions: " << theDB()->tranlist().size() << "</p>\r\n"
			<< "<p>Cursors: " << cursors_inuse << "</p>\r\n"
			<< "<p>Database Size: " << MB(theDB()->mmf->size()) << "mb</p>\r\n"
			<< "<p>Connections: (" << conns.size() << ") ";
		for (SuObject::iterator iter = conns.begin();  iter != conns.end(); ++iter)
				page << (iter == conns.begin() ? "" : " + ")
					<< (*iter).second.gcstr();
		page << "</p>\r\n"
			<< "</body>\r\n"
			<< "</html>\r\n";

		time_t t;
		time(&t);
		struct tm* today = localtime(&t);
		char date[128];
		strftime(date, 128, "%a, %d %b %Y %H:%m:%S", today);

		OstreamStr hdr;
		hdr << "HTTP/1.0 200 OK\r\n"
			<< "Server: Suneido\r\n"
			<< "Content-Type: text/html\r\n"
			<< "Content-Length: " << page.size() << "\r\n"
			<< "Last-Modified: " << date << "\r\n"
			<< "Date: " << date << "\r\n"
			<< "Expires: " << date << "\r\n"
			<< "\r\n";
		sc->writebuf(hdr.str(), hdr.size());
		sc->write(page.str(), page.size());
		sc->close();
		}
	catch (...)
		{
		}
	}

