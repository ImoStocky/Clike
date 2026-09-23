#include "compiler.h"

#include <stdlib.h>
#include <string.h>

void compiler_init(compiler_t *c)
{
	memset(c, 0, sizeof(*c));
}

void compiler_fail(compiler_t *c, errv_t status)
{
	if (c->status == COMP_OK)
		c->status = status;
}

void compiler_cleanup(compiler_t *c)
{
	ast_t *node = c->nodes;
	while (node != NULL)
	{
		ast_t *next = node->track;
		free(node->name);
		free(node->sval);
		free(node);
		node = next;
	}
	c->nodes = NULL;
	c->prog = NULL;
	if (c->tok.type == IDENT_TK || c->tok.type == LITERAL_TK)
		free(c->tok.data.lit);
	c->tok.data.lit = NULL;
	scanner_destroy(&c->scan);
	if (c->own_in && c->in != NULL)
		fclose(c->in);
	c->in = NULL;
	c->own_in = 0;
}
