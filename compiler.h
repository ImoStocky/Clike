#ifndef IFJ_COMPILER_H
#define IFJ_COMPILER_H

#include "ast.h"
#include "lexal.h"

struct compiler_s
{
	errv_t status;
	FILE *in;
	int own_in;
	scanner_t scan;
	token_t tok;
	ast_t *prog;
	ast_t *nodes;
};

void compiler_init(compiler_t *c);
void compiler_fail(compiler_t *c, errv_t status);
void compiler_cleanup(compiler_t *c);

#endif
