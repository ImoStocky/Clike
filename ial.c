#include "ial.h"
#include "types.h"

#include <stdlib.h>
#include <string.h>

static unsigned sdbm_hash(const char *str)
{
	unsigned hash = 0;
	unsigned char c;
	while ((c = (unsigned char)*str++) != 0)
		hash = c + (hash << 6) + (hash << 16) - hash;
	return hash;
}

htab_t *htab_init(unsigned n)
{
	htab_t *t = xmalloc(sizeof(*t));
	t->n = n ? n : 64;
	t->b = xmalloc(sizeof(hnode_t *) * t->n);
	memset(t->b, 0, sizeof(hnode_t *) * t->n);
	return t;
}

void htab_free(htab_t *t, void (*free_val)(void *))
{
	unsigned i;
	if (t == NULL)
		return;
	for (i = 0; i < t->n; i++)
	{
		hnode_t *p = t->b[i];
		while (p != NULL)
		{
			hnode_t *n = p->next;
			free(p->key);
			if (free_val)
				free_val(p->val);
			free(p);
			p = n;
		}
	}
	free(t->b);
	free(t);
}

int htab_put(htab_t *t, const char *key, void *val)
{
	unsigned i = sdbm_hash(key) % t->n;
	hnode_t *p = t->b[i];
	while (p != NULL)
	{
		if (strcmp(p->key, key) == 0)
		{
			p->val = val;
			return 1;
		}
		p = p->next;
	}
	p = xmalloc(sizeof(*p));
	p->key = xstrdup(key);
	p->val = val;
	p->next = t->b[i];
	t->b[i] = p;
	return 0;
}

void *htab_get(htab_t *t, const char *key)
{
	unsigned i;
	hnode_t *p;
	if (t == NULL)
		return NULL;
	i = sdbm_hash(key) % t->n;
	p = t->b[i];
	while (p != NULL)
	{
		if (strcmp(p->key, key) == 0)
			return p->val;
		p = p->next;
	}
	return NULL;
}

int kmp_find(const char *text, const char *pat)
{
	int n, m, i, j;
	int *pi;
	if (text == NULL || pat == NULL)
		return -1;
	n = (int)strlen(text);
	m = (int)strlen(pat);
	if (m == 0)
		return 0;
	pi = xmalloc(sizeof(int) * (size_t)m);
	pi[0] = 0;
	j = 0;
	for (i = 1; i < m; i++)
	{
		while (j > 0 && pat[i] != pat[j])
			j = pi[j - 1];
		if (pat[i] == pat[j])
			j++;
		pi[i] = j;
	}
	j = 0;
	for (i = 0; i < n; i++)
	{
		while (j > 0 && text[i] != pat[j])
			j = pi[j - 1];
		if (text[i] == pat[j])
			j++;
		if (j == m)
		{
			free(pi);
			return i - m + 1;
		}
	}
	free(pi);
	return -1;
}

static void heapify(char *s, int n, int i)
{
	int largest = i;
	int l = 2 * i + 1;
	int r = 2 * i + 2;
	if (l < n && (unsigned char)s[l] > (unsigned char)s[largest])
		largest = l;
	if (r < n && (unsigned char)s[r] > (unsigned char)s[largest])
		largest = r;
	if (largest != i)
	{
		char tmp = s[i];
		s[i] = s[largest];
		s[largest] = tmp;
		heapify(s, n, largest);
	}
}

void heap_sort_chars(char *s)
{
	int n, i;
	if (s == NULL)
		return;
	n = (int)strlen(s);
	for (i = n / 2 - 1; i >= 0; i--)
		heapify(s, n, i);
	for (i = n - 1; i > 0; i--)
	{
		char tmp = s[0];
		s[0] = s[i];
		s[i] = tmp;
		heapify(s, i, 0);
	}
}
