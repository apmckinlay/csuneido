void register_tls(void** pvar);

struct Tls
	{
	Tls(void** pvar)
		{
		register_tls(pvar);
		}
	};

#define TLS(var) Tls tls_##var(reinterpret_cast<void**>(&var))
