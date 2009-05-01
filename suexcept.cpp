#include "sustring.h"

class SuExcept : public SuString
	{
public:
	SuExcept(char* s) : string(s), calls(copyCallStack())
		{ }
private:
	SuObject* SuExcept::copyCallStack();

	SuObject* calls;
	}

#define TRY(stuff) do try { stuff } catch (...) { } while (false)

SuObject* SuExcept::copyCallStack()
	{
	SuObject* calls = new SuObject;
	for (Frame* f = tss_proc()->fp; f > tss_proc()->frames; --f)
		{
		SuObject* call = new SuObject;
		SuObject* vars = new SuObject;
		if (f->fn)
			{
			TRY(call->put("fn", f->fn))
			int i, n;
			TRY(i = f->fn->source(f->ip - f->fn->code - 1, &n));
			call->put("src_i", i);
			call->put("src_n", n);
			for (i = 0; i < f->fn->nlocals; ++i)
				if (f->local[i])
					TRY(vars->put(symbol(f->fn->locals[i]), f->local[i]));
			if (f->self)
				vars->put("this", f->self);
			}

		else
			{
			TRY(call->put("fn", f->prim));
			}

		call->put("locals", vars);
		calls->add(call);
		}
	return calls;
	}
