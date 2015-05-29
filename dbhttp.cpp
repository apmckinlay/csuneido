/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 * 
 * Copyright (c) 2001 Suneido Software Corp. 
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
	void run();
private:
	void request(char* buf);

	SocketConnect* sc;
	};

static void _stdcall dbhttp(void* sc)
	{
	DbHttp dbh((SocketConnect*) sc);
	dbh.run();
	Fibers::end();
	}

void start_dbhttp()
	{
	extern int su_port;
	socketServer("", su_port + 1, dbhttp, 0, false);
	}

#define MB(n) ((n + 512 * 1024) / (1024*1024))

void DbHttp::run()
	{
	try
		{
		const int bufsize = 512;
		char buf[bufsize];
		sc->readline(buf, bufsize);

		extern SuObject& dbserver_connections();
		SuObject& conns = dbserver_connections();
		conns.sort();
		extern int tempdest_inuse;
		extern int cursors_inuse;
		OstreamStr page;
		page << "<html>\r\n"
			<< "<head>\r\n"
			<< "<title>Suneido Server Monitor</title>\r\n"
			<< "<meta http-equiv=\"refresh\" content=\"15\" />\r\n"
			<< "</head>\r\n"
			<< "<body>\r\n"
			<< "<h1>Suneido Server Monitor</h1>\r\n"
			<< "<p>Built: " << build_date << "</p>\r\n"
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

