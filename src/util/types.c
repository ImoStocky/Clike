#include "types.h"

#include <stdio.h>
#include <string.h>

void *xmalloc(size_t n)
{
	void *p = malloc(n);
	if (p == NULL)
		exit(INTER_ERR);
	return p;
}

char *xstrdup(const char *s)
{
	size_t n;
	char *d;
	if (s == NULL)
		return NULL;
	n = strlen(s) + 1;
	d = xmalloc(n);
	memcpy(d, s, n);
	return d;
}

void die(errv_t e)
{
	exit((int)e);
}
