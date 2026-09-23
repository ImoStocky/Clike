#include "ast.h"

#include <string.h>

ast_t *ast_new(ast_kind_t kind)
{
	ast_t *n = xmalloc(sizeof(*n));
	memset(n, 0, sizeof(*n));
	n->kind = kind;
	n->dtype = TY_VOID;
	return n;
}

void ast_append(ast_t **head, ast_t *node)
{
	if (*head == NULL)
	{
		*head = node;
		return;
	}
	ast_t *p = *head;
	while (p->next != NULL)
		p = p->next;
	p->next = node;
}
