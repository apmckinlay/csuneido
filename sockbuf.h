class SockBuf
	{
public:
	explicit SockBuf(int n = 4000);
	char* reserve(int n);
	void add(char* s, int n);
	void remove(int n);
	int size()
		{ return siz; }
	char* buffer()
		{ return buf; }
	void added(int n)
		{ siz += n; }
	void clear()
		{ siz = 0; }
private:
	int cap;
	int siz;
	char* buf;
	};
