#ifndef IFJ_IAL_H
#define IFJ_IAL_H

typedef struct hnode_s
{
	char *key;
	void *val;
	struct hnode_s *next;
} hnode_t;

typedef struct htab_s
{
	hnode_t **b;
	unsigned n;
} htab_t;

htab_t *htab_init(unsigned n);
void htab_free(htab_t *t, void (*free_val)(void *));
int htab_put(htab_t *t, const char *key, void *val);
void *htab_get(htab_t *t, const char *key);

int kmp_find(const char *text, const char *pat);
void heap_sort_chars(char *s);

#endif
