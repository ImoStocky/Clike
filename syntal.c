#include "syntal.h"
#include "compiler.h"


#include <stdio.h>
#include <string.h>

static int next(compiler_t *c)
{
	errv_t status;
	if (c->status != COMP_OK)
		return 0;
	if (c->tok.type == IDENT_TK || c->tok.type == LITERAL_TK)
		free(c->tok.data.lit);
	c->tok.data.lit = NULL;
	status = scanner_generateToken(&c->scan, &c->tok);
	if (status != COMP_OK)
	{
		compiler_fail(c, status);
		return 0;
	}
	return 1;
}

static int is_type_kw(compiler_t *c, token_kind_t t)
{
	(void)c;
	return t == INT_KW || t == DOUBLE_KW || t == STRING_KW || t == AUTO_KW;
}

static dtype_t type_of_kw(compiler_t *c, token_kind_t t)
{
	if (t == INT_KW) return TY_INT;
	if (t == DOUBLE_KW) return TY_DOUBLE;
	if (t == STRING_KW) return TY_STRING;
	if (t == AUTO_KW) return TY_AUTO;
	compiler_fail(c, SYN_ERR);
	return TY_VOID;
}

static int expect(compiler_t *c, token_kind_t k)
{
	if (c->status != COMP_OK)
		return 0;
	if (c->tok.type != k)
	{
		compiler_fail(c, SYN_ERR);
		return 0;
	}
	return next(c);
}

static ast_t *parse_stmt(compiler_t *c);
static ast_t *parse_block(compiler_t *c);
static ast_t *parse_expr(compiler_t *c);
static ast_t *parse_or(compiler_t *c);

static ast_t *parse_args(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *list = NULL;
	if (c->tok.type == BRACER_OP)
		return NULL;
	for (;;)
	{
		if (c->status != COMP_OK)
			break;
		ast_append(&list, parse_expr(c));
		if (c->tok.type == COM_OP)
		{
			next(c);
			continue;
		}
		break;
	}
	return list;
}

static ast_t *parse_primary(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n;
	if (c->tok.type == INT_TK)
	{
		n = ast_new(c, AST_INT);
		n->ival = c->tok.data.ord;
		n->dtype = TY_INT;
		next(c);
		return n;
	}
	if (c->tok.type == REAL_TK)
	{
		n = ast_new(c, AST_DOUBLE);
		n->dval = c->tok.data.real;
		n->dtype = TY_DOUBLE;
		next(c);
		return n;
	}
	if (c->tok.type == LITERAL_TK)
	{
		n = ast_new(c, AST_STRING);
		n->sval = c->tok.data.lit;
		c->tok.data.lit = NULL;
		n->dtype = TY_STRING;
		next(c);
		return n;
	}
	if (c->tok.type == IDENT_TK)
	{
		char *name = c->tok.data.lit;
		c->tok.data.lit = NULL;
		next(c);
		if (c->tok.type == BRACEL_OP)
		{
			n = ast_new(c, AST_CALL);
			n->name = name;
			next(c);
			n->a = parse_args(c);
			expect(c, BRACER_OP);
			return n;
		}
		n = ast_new(c, AST_IDENT);
		n->name = name;
		return n;
	}
	if (c->tok.type == BRACEL_OP)
	{
		next(c);
		n = parse_expr(c);
		expect(c, BRACER_OP);
		return n;
	}
	{ compiler_fail(c, SYN_ERR); return NULL; }
	return NULL;
}

static ast_t *parse_unary(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	if (c->tok.type == MINUS_OP || c->tok.type == NOT_OP)
	{
		ast_t *n = ast_new(c, AST_UNOP);
		n->op = c->tok.type;
		next(c);
		n->a = parse_unary(c);
		return n;
	}
	return parse_primary(c);
}

static ast_t *binop(compiler_t *c, ast_t *l, token_kind_t op, ast_t *r)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n = ast_new(c, AST_BINOP);
	n->op = op;
	n->a = l;
	n->b = r;
	return n;
}

static ast_t *parse_mul(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *l = parse_unary(c);
	while (c->status == COMP_OK && (c->tok.type == ASTERISK_OP || c->tok.type == SLASH_OP || c->tok.type == PERCENT_OP))
	{
		token_kind_t op = c->tok.type;
		next(c);
		l = binop(c, l, op, parse_unary(c));
	}
	return l;
}

static ast_t *parse_add(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *l = parse_mul(c);
	while (c->status == COMP_OK && (c->tok.type == PLUS_OP || c->tok.type == MINUS_OP))
	{
		token_kind_t op = c->tok.type;
		next(c);
		l = binop(c, l, op, parse_mul(c));
	}
	return l;
}

static ast_t *parse_rel(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *l = parse_add(c);
	while (c->status == COMP_OK && (c->tok.type == LESS_OP || c->tok.type == GREAT_OP || c->tok.type == LEE_OP || c->tok.type == GRE_OP))
	{
		token_kind_t op = c->tok.type;
		next(c);
		l = binop(c, l, op, parse_add(c));
	}
	return l;
}

static ast_t *parse_eq(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *l = parse_rel(c);
	while (c->status == COMP_OK && (c->tok.type == EQ_OP || c->tok.type == NEQ_OP))
	{
		token_kind_t op = c->tok.type;
		next(c);
		l = binop(c, l, op, parse_rel(c));
	}
	return l;
}

static ast_t *parse_and(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *l = parse_eq(c);
	while (c->status == COMP_OK && c->tok.type == AND_OP)
	{
		next(c);
		l = binop(c, l, AND_OP, parse_eq(c));
	}
	return l;
}

static ast_t *parse_or(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *l = parse_and(c);
	while (c->status == COMP_OK && c->tok.type == OR_OP)
	{
		next(c);
		l = binop(c, l, OR_OP, parse_and(c));
	}
	return l;
}

static ast_t *parse_expr(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	return parse_or(c);
}

static ast_t *parse_vardecl_body(compiler_t *c, dtype_t dt, char *name, int need_semi)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n = ast_new(c, AST_VARDECL);
	n->dtype = dt;
	n->name = name;
	if (c->tok.type == ASSIGN_OP)
	{
		next(c);
		n->a = parse_expr(c);
	}
	if (need_semi)
		expect(c, SEMI_OP);
	return n;
}

static ast_t *parse_assign_or_call_stmt(compiler_t *c, char *name)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n;
	if (c->tok.type == ASSIGN_OP)
	{
		n = ast_new(c, AST_ASSIGN);
		n->name = name;
		next(c);
		n->a = parse_expr(c);
		expect(c, SEMI_OP);
		return n;
	}
	if (c->tok.type == BRACEL_OP)
	{
		n = ast_new(c, AST_EXPRSTMT);
		n->a = ast_new(c, AST_CALL);
		n->a->name = name;
		next(c);
		n->a->a = parse_args(c);
		expect(c, BRACER_OP);
		expect(c, SEMI_OP);
		return n;
	}
	{ compiler_fail(c, SYN_ERR); return NULL; }
	return NULL;
}

static ast_t *parse_if(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n = ast_new(c, AST_IF);
	next(c);
	expect(c, BRACEL_OP);
	n->a = parse_expr(c);
	expect(c, BRACER_OP);
	n->b = parse_stmt(c);
	if (c->tok.type == ELSE_KW)
	{
		next(c);
		n->c = parse_stmt(c);
	}
	return n;
}

static ast_t *parse_for(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n = ast_new(c, AST_FOR);
	next(c);
	expect(c, BRACEL_OP);
	if (is_type_kw(c, c->tok.type))
	{
		dtype_t dt = type_of_kw(c, c->tok.type);
		char *name;
		next(c);
		if (c->tok.type != IDENT_TK)
			{ compiler_fail(c, SYN_ERR); return NULL; }
		name = c->tok.data.lit;
		c->tok.data.lit = NULL;
		next(c);
		n->a = parse_vardecl_body(c, dt, name, 0);
		expect(c, SEMI_OP);
	}
	else if (c->tok.type == IDENT_TK)
	{
		char *name = c->tok.data.lit;
		c->tok.data.lit = NULL;
		next(c);
		if (c->tok.type != ASSIGN_OP)
			{ compiler_fail(c, SYN_ERR); return NULL; }
		n->a = ast_new(c, AST_ASSIGN);
		n->a->name = name;
		next(c);
		n->a->a = parse_expr(c);
		expect(c, SEMI_OP);
	}
	else
		expect(c, SEMI_OP);

	if (c->tok.type != SEMI_OP)
		n->b = parse_expr(c);
	expect(c, SEMI_OP);

	if (c->tok.type != BRACER_OP)
	{
		if (c->tok.type != IDENT_TK)
			{ compiler_fail(c, SYN_ERR); return NULL; }
		n->c = ast_new(c, AST_ASSIGN);
		n->c->name = c->tok.data.lit;
		c->tok.data.lit = NULL;
		next(c);
		expect(c, ASSIGN_OP);
		n->c->a = parse_expr(c);
	}
	expect(c, BRACER_OP);
	n->d = parse_stmt(c);
	return n;
}

static ast_t *parse_while(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n = ast_new(c, AST_WHILE);
	next(c);
	expect(c, BRACEL_OP);
	n->a = parse_expr(c);
	expect(c, BRACER_OP);
	n->b = parse_stmt(c);
	return n;
}

static ast_t *parse_do(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n = ast_new(c, AST_DO);
	next(c);
	n->a = parse_stmt(c);
	expect(c, WHILE_KW);
	expect(c, BRACEL_OP);
	n->b = parse_expr(c);
	expect(c, BRACER_OP);
	expect(c, SEMI_OP);
	return n;
}

static ast_t *parse_try(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n = ast_new(c, AST_TRY);
	next(c);
	n->a = parse_stmt(c);
	expect(c, CATCH_KW);
	expect(c, BRACEL_OP);
	if (c->tok.type != STRING_KW)
		{ compiler_fail(c, SYN_ERR); return NULL; }
	n->dtype = TY_STRING;
	next(c);
	if (c->tok.type != IDENT_TK)
		{ compiler_fail(c, SYN_ERR); return NULL; }
	n->name = c->tok.data.lit;
	c->tok.data.lit = NULL;
	next(c);
	expect(c, BRACER_OP);
	n->b = parse_stmt(c);
	return n;
}

static ast_t *parse_throw(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n = ast_new(c, AST_THROW);
	next(c);
	n->a = parse_expr(c);
	expect(c, SEMI_OP);
	return n;
}

static ast_t *parse_cin(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n = ast_new(c, AST_CIN);
	next(c);
	expect(c, DBL_GRE_OP);
	if (c->tok.type != IDENT_TK)
		{ compiler_fail(c, SYN_ERR); return NULL; }
	do
	{
		ast_t *id = ast_new(c, AST_IDENT);
		id->name = c->tok.data.lit;
		c->tok.data.lit = NULL;
		ast_append(&n->a, id);
		next(c);
		if (c->tok.type == DBL_GRE_OP)
			next(c);
		else
			break;
	} while (c->status == COMP_OK && c->tok.type == IDENT_TK);
	expect(c, SEMI_OP);
	return n;
}

static ast_t *parse_cout(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n = ast_new(c, AST_COUT);
	next(c);
	expect(c, DBL_LESS_OP);
	ast_append(&n->a, parse_expr(c));
	while (c->status == COMP_OK && c->tok.type == DBL_LESS_OP)
	{
		next(c);
		ast_append(&n->a, parse_expr(c));
	}
	expect(c, SEMI_OP);
	return n;
}

static ast_t *parse_block(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n = ast_new(c, AST_BLOCK);
	expect(c, BLOCKL_OP);
	while (c->status == COMP_OK && c->tok.type != BLOCKR_OP)
	{
		if (c->tok.type == END_TK)
			{ compiler_fail(c, SYN_ERR); return NULL; }
		ast_append(&n->a, parse_stmt(c));
	}
	expect(c, BLOCKR_OP);
	return n;
}

static ast_t *parse_stmt(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	if (c->tok.type == BLOCKL_OP)
		return parse_block(c);
	if (c->tok.type == SEMI_OP)
	{
		next(c);
		return ast_new(c, AST_BLOCK);
	}
	if (is_type_kw(c, c->tok.type))
	{
		dtype_t dt = type_of_kw(c, c->tok.type);
		char *name;
		next(c);
		if (c->tok.type != IDENT_TK)
			{ compiler_fail(c, SYN_ERR); return NULL; }
		name = c->tok.data.lit;
		c->tok.data.lit = NULL;
		next(c);
		return parse_vardecl_body(c, dt, name, 1);
	}
	if (c->tok.type == IF_KW)
		return parse_if(c);
	if (c->tok.type == FOR_KW)
		return parse_for(c);
	if (c->tok.type == WHILE_KW)
		return parse_while(c);
	if (c->tok.type == DO_KW)
		return parse_do(c);
	if (c->tok.type == TRY_KW)
		return parse_try(c);
	if (c->tok.type == THROW_KW)
		return parse_throw(c);
	if (c->tok.type == RETURN_KW)
	{
		ast_t *n = ast_new(c, AST_RETURN);
		next(c);
		if (c->tok.type != SEMI_OP)
			n->a = parse_expr(c);
		expect(c, SEMI_OP);
		return n;
	}
	if (c->tok.type == CIN_KW)
		return parse_cin(c);
	if (c->tok.type == COUT_KW)
		return parse_cout(c);
	if (c->tok.type == IDENT_TK)
	{
		char *name = c->tok.data.lit;
		c->tok.data.lit = NULL;
		next(c);
		return parse_assign_or_call_stmt(c, name);
	}
	{ compiler_fail(c, SYN_ERR); return NULL; }
	return NULL;
}

static ast_t *parse_params(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *list = NULL;
	if (c->tok.type == BRACER_OP)
		return NULL;
	for (;;)
	{
		ast_t *p;
		if (c->status != COMP_OK)
			break;
		if (!is_type_kw(c, c->tok.type))
			{ compiler_fail(c, SYN_ERR); return NULL; }
		p = ast_new(c, AST_PARAM);
		p->dtype = type_of_kw(c, c->tok.type);
		if (p->dtype == TY_AUTO)
			{ compiler_fail(c, SEM_OTHER_ERR); return NULL; }
		next(c);
		if (c->tok.type != IDENT_TK)
			{ compiler_fail(c, SYN_ERR); return NULL; }
		p->name = c->tok.data.lit;
		c->tok.data.lit = NULL;
		next(c);
		ast_append(&list, p);
		if (c->tok.type == COM_OP)
		{
			next(c);
			continue;
		}
		break;
	}
	return list;
}

static ast_t *parse_func(compiler_t *c)
{
	if (c->status != COMP_OK)
		return NULL;
	ast_t *n;
	if (!is_type_kw(c, c->tok.type))
		{ compiler_fail(c, SYN_ERR); return NULL; }
	n = ast_new(c, AST_FUNC);
	n->dtype = type_of_kw(c, c->tok.type);
	if (n->dtype == TY_AUTO)
		{ compiler_fail(c, SEM_OTHER_ERR); return NULL; }
	next(c);
	if (c->tok.type != IDENT_TK)
		{ compiler_fail(c, SYN_ERR); return NULL; }
	n->name = c->tok.data.lit;
	c->tok.data.lit = NULL;
	next(c);
	expect(c, BRACEL_OP);
	n->a = parse_params(c);
	expect(c, BRACER_OP);
	if (c->tok.type == SEMI_OP)
	{
		next(c);
		return n;
	}
	n->b = parse_block(c);
	return n;
}

errv_t parse_program(compiler_t *c)
{
	c->prog = ast_new(c, AST_PROG);
	if (!next(c))
		return c->status;
	if (c->tok.type == END_TK)
	{
		compiler_fail(c, SYN_ERR);
		return c->status;
	}
	while (c->status == COMP_OK && c->tok.type != END_TK)
	{
		ast_append(&c->prog->a, parse_func(c));
		if (c->status != COMP_OK)
			break;
	}
	return c->status;
}
