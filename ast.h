#ifndef IFJ_AST_H
#define IFJ_AST_H

#include "types.h"

typedef enum ast_kind_e
{
	AST_PROG,
	AST_FUNC,
	AST_PARAM,
	AST_BLOCK,
	AST_VARDECL,
	AST_IF,
	AST_FOR,
	AST_WHILE,
	AST_DO,
	AST_RETURN,
	AST_ASSIGN,
	AST_CIN,
	AST_COUT,
	AST_EXPRSTMT,
	AST_BINOP,
	AST_UNOP,
	AST_CALL,
	AST_IDENT,
	AST_INT,
	AST_DOUBLE,
	AST_STRING
} ast_kind_t;

typedef struct ast_s
{
	ast_kind_t kind;
	dtype_t dtype;
	token_kind_t op;
	char *name;
	int ival;
	double dval;
	char *sval;
	struct ast_s *a;
	struct ast_s *b;
	struct ast_s *c;
	struct ast_s *d;
	struct ast_s *next;
} ast_t;

ast_t *ast_new(ast_kind_t kind);
void ast_append(ast_t **head, ast_t *node);

#endif
