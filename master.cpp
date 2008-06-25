#include "win.h"
#include "ostreamfile.h"

class Master
	{
public:
	static void run();
	static void open();
	};
static char* slaveip;

DWORD WINAPI master_thread(void* p)
	{
	Master::run();
	return 0; 
	}

void master(char* ip)
	{
	slaveip = ip;
	CreateThread(NULL, 0, master_thread, 0, 0, NULL);
	}

void Master::run()
	{
	OstreamFile out(slaveip);
	out << "starting" << endl;

	//~ HANDLE f = CreateFile("suneido.db",
		//~ GENERIC_READ,
		//~ FILE_SHARE_READ,
		//~ NULL, // no security attributes
		//~ OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
		//~ NULL); // no template
//	Mmfile mmf(f);
//	mmf.set_max_chunks_mapped(16);
	
//	Mmoffset size = mmf.get_file_size();
	while (true)
//		if (size != mmf.get_file_size())
//			out << (size = mmf.get_file_size()) << endl;
//		else
			Sleep(1);
	}
