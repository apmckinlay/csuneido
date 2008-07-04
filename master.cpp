#include "recover.h"
#include "mmfile.h"
#include "mmtypes.h"
#include "checksum.h"
#include "ostreamfile.h"
#include "database.h"
#include "value.h"
#include "win.h" // for sleep
#include "fatal.h"

class Master
	{
public:
	Master(char* ip) : 
		slaveip(ip), mmf("suneido.db", false, true), cksum(checksum(0, 0, 0)),tries(0)
,out(slaveip)
		{ }
	void run();
private:
	void poll();
	void newstuff();
	void handle_commit(void* p);
	void print(Mmoffset off);
	void Master::print_record(Record& r);

	char* slaveip;
	Mmfile mmf;
	Mmoffset prevsize;
	Mmoffset newsize;
	ulong cksum;
	int tries;
OstreamFile out;
	};

void master(char* slaveip)
	{
	Master master(slaveip);
	master.run();
	}

void Master::run()
	{
	prevsize = mmf.get_file_size();
	poll();
	}
	
void Master::poll()
	{
	while (true)
		{
		mmf.refresh();
		if (prevsize != (newsize = mmf.size()))
			newstuff();
		else
			Sleep(1);
		}
	}

void Master::newstuff()
	{
	if (++tries > 10)
		fatal("too many retries");
	out << "newstuff" << endl;
	Mmfile::iterator end(mmf.end());
	for (Mmfile::iterator iter(prevsize + MM_HEADER, &mmf); iter != end; ++iter)
		try
			{
			out << "offset " << iter.offset() << 
				" type " << (int) iter.type() << 
				" size " << iter.size() <<
				endl;
			
			if (iter.type() == MM_DATA)
				{
				int tblnum = *(int*) *iter;
				if (tblnum != TN_TABLES && tblnum != TN_INDEXES)
					{
					Record r(&mmf, iter.offset() + sizeof (int));
					cksum = checksum(cksum, *iter, sizeof (int) + r.cursize());
					}
				print(iter.offset() + sizeof (int));
				}
			else if (iter.type() == MM_COMMIT)
				{
				Commit* commit = (Commit*) *iter;
				cksum = checksum(cksum, (char*) commit + sizeof (long), 
					iter.size() - sizeof (long));
				if (commit->cksum != cksum)
					{
					out << "checksum doesn't match!" << endl;
					Sleep(1);
					return ; // retry
					}
				cksum = checksum(0, 0, 0);
				handle_commit(*iter);
				prevsize = iter.offset() + iter.size() + MM_TRAILER;
				}
			}
		catch (const Except& x)
			{
			out << x << endl;
			Sleep(1);
			return ; // retry
			}
	prevsize = newsize;
	tries = 0;
	}

void Master::handle_commit(void* p)
	{
	Commit* commit = (Commit*) p;
	Mmoffset32* deletes = commit->deletes();
	for (int i = 0; i < commit->ndeletes; ++i)
		{
		out << "DELETE ";
		print(deletes[i].unpack());
		}
	Mmoffset32* creates = commit->creates();
	for (int i = 0; i < commit->ncreates; ++i)
		{
		out << "CREATE ";
		print(creates[i].unpack());
		}
	}

void Master::print(Mmoffset off)
	{
	TblNum tn = ((int*) mmf.adr(off))[-1];
	Record r(&mmf, off);
	out << "table " << tn << " =";
	print_record(r);
	}
	
void Master::print_record(Record& r)
	{
	int f;
	try
		{
		out << " #" << r.size();
		for (f = 0; f < r.size(); ++f)
			out << " " << r.getval(f);
		}
	catch (const Except& e)
		{
		out << " " << f << " " << e;
		}
	out << endl;
	}
