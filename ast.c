#include "ast.h"
#include "compiler.h"

#include <stdlib.h>
#include <string.h>

ast_t *ast_try_body(ast_t *node)
{
	return node->a;
}

ast_t *ast_try_handler(ast_t *node)
{
	return node->b;
}

ast_t *ast_throw_expr(ast_t *node)
{
	return node->a;
}

ast_t *ast_new(compiler_t *c, ast_kind_t kind)
{
	ast_t *n = xmalloc(sizeof(*n));
	memset(n, 0, sizeof(*n));
	n->kind = kind;
	n->dtype = TY_VOID;
	n->track = c->nodes;
	c->nodes = n;
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

void ast_free(ast_t *node)
{
	if (node == NULL)
		return;
	ast_free(node->a);
	ast_free(node->b);
	ast_free(node->c);
	ast_free(node->d);
	ast_free(node->next);
	free(node->name);
	free(node->sval);
	free(node);
}
