class CodeVisitor
	{
public:
	virtual void local(int pos, int i, bool init)
		{ }
	virtual void global(int pos, int gnum)
		{ }
	virtual void begin_func()
		{ }
	virtual void end_func()
		{ }
	virtual ~CodeVisitor()
		{ } // to avoid warnings
	};
