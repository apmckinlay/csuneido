// Copyright (c) 2000 Suneido Software Corp. All rights reserved
// Licensed under GPLv2

static bool isdigit(char c)
	{
	return '0' <= c && c <= '9';
	}

// does NOT handle hex
int numlen(const char* s)
	{
	const char* start = s;

	if (*s == '+' || *s == '-')
		++s;
	int intdigits = isdigit(*s); // really bool
	while (isdigit(*s))
		++s;
	if (*s == '.')
		++s;
	int fracdigits = isdigit(*s); // really bool
	while (isdigit(*s))
		++s;
	if (! intdigits && ! fracdigits)
		return -1;
	if ((*s == 'e' || *s == 'E') &&
		(isdigit(s[1]) || ((s[1] == '-' || s[1] == '+') && isdigit(s[2]))))
		{
		s += ((s[1] == '-' || s[1] == '+') ? 3 : 2);
		while (isdigit(*s))
			++s;
		}
	return s - start;
	}
