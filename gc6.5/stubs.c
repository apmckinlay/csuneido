void GC_push_all_stack(void* bottom, void* top);

void GC_push_all_stacks()
	{
	void* p;
#if defined(_MSC_VER)
	_asm
		{
		mov eax,fs:4
		mov p,eax
		}
#elif defined(__GNUC__)
	asm("movl %%fs:0x4,%%eax" : "=a" (p));
#else
#	warning "replacement for inline assembler required"
#endif
	GC_push_all_stack(&p, p);
	}

void GC_push_thread_structures()
	{
	}
