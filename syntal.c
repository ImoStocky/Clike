#include "syntal.h"
#include "lexal.h"
#include "types.h"

#include <stdio.h>
#include <string.h>

static token_t tok;

static void next(void)
{
	scanner_generateToken(&tok);
}

static int is_type_kw(token_kind_t t)
{
	return t == INT_KW || t == DOUBLE_KW || t == STRING_KW || t == AUTO_KW;
}

static dtype_t type_of_kw(token_kind_t t)
{
	if (t == INT_KW) return TY_INT;
	if (t == DOUBLE_KW) return TY_DOUBLE;
	if (t == STRING_KW) return TY_STRING;
	if (t == AUTO_KW) return TY_AUTO;
	die(SYN_ERR);
	return TY_VOID;
}

static void expect(token_kind_t k)
{
	if (tok.type != k)
		die(SYN_ERR);
	next();
}

static ast_t *parse_stmt(void);
static ast_t *parse_block(void);
static ast_t *parse_expr(void);
static ast_t *parse_or(void);

static ast_t *parse_args(void)
{
	ast_t *list = NULL;
	if (tok.type == BRACER_OP)
		return NULL;
	for (;;)
	{
		ast_append(&list, parse_expr());
		if (tok.type == COM_OP)
		{
			next();
			continue;
		}
		break;
	}
	return list;
}

static ast_t *parse_primary(void)
{
	ast_t *n;
	if (tok.type == INT_TK)
	{
		n = ast_new(AST_INT);
		n->ival = tok.data.ord;
		n->dtype = TY_INT;
		next();
		return n;
	}
	if (tok.type == REAL_TK)
	{
		n = ast_new(AST_DOUBLE);
		n->dval = tok.data.real;
		n->dtype = TY_DOUBLE;
		next();
		return n;
	}
	if (tok.type == LITERAL_TK)
	{
		n = ast_new(AST_STRING);
		n->sval = tok.data.lit;
		tok.data.lit = NULL;
		n->dtype = TY_STRING;
		next();
		return n;
	}
	if (tok.type == IDENT_TK)
	{
		char *name = tok.data.lit;
		tok.data.lit = NULL;
		next();
		if (tok.type == BRACEL_OP)
		{
			n = ast_new(AST_CALL);
			n->name = name;
			next();
			n->a = parse_args();
			expect(BRACER_OP);
			return n;
		}
		n = ast_new(AST_IDENT);
		n->name = name;
		return n;
	}
	if (tok.type == BRACEL_OP)
	{
		next();
		n = parse_expr();
		expect(BRACER_OP);
		return n;
	}
	die(SYN_ERR);
	return NULL;
}

static ast_t *parse_unary(void)
{
	if (tok.type == MINUS_OP || tok.type == NOT_OP)
	{
		ast_t *n = ast_new(AST_UNOP);
		n->op = tok.type;
		next();
		n->a = parse_unary();
		return n;
	}
	return parse_primary();
}

static ast_t *binop(ast_t *l, token_kind_t op, ast_t *r)
{
	ast_t *n = ast_new(AST_BINOP);
	n->op = op;
	n->a = l;
	n->b = r;
	return n;
}

static ast_t *parse_mul(void)
{
	ast_t *l = parse_unary();
	while (tok.type == ASTERISK_OP || tok.type == SLASH_OP || tok.type == PERCENT_OP)
	{
		token_kind_t op = tok.type;
		next();
		l = binop(l, op, parse_unary());
	}
	return l;
}

static ast_t *parse_add(void)
{
	ast_t *l = parse_mul();
	while (tok.type == PLUS_OP || tok.type == MINUS_OP)
	{
		token_kind_t op = tok.type;
		next();
		l = binop(l, op, parse_mul());
	}
	return l;
}

static ast_t *parse_rel(void)
{
	ast_t *l = parse_add();
	while (tok.type == LESS_OP || tok.type == GREAT_OP || tok.type == LEE_OP || tok.type == GRE_OP)
	{
		token_kind_t op = tok.type;
		next();
		l = binop(l, op, parse_add());
	}
	return l;
}

static ast_t *parse_eq(void)
{
	ast_t *l = parse_rel();
	while (tok.type == EQ_OP || tok.type == NEQ_OP)
	{
		token_kind_t op = tok.type;
		next();
		l = binop(l, op, parse_rel());
	}
	return l;
}

static ast_t *parse_and(void)
{
	ast_t *l = parse_eq();
	while (tok.type == AND_OP)
	{
		next();
		l = binop(l, AND_OP, parse_eq());
	}
	return l;
}

static ast_t *parse_or(void)
{
	ast_t *l = parse_and();
	while (tok.type == OR_OP)
	{
		next();
		l = binop(l, OR_OP, parse_and());
	}
	return l;
}

static ast_t *parse_expr(void)
{
	return parse_or();
}

static ast_t *parse_vardecl_body(dtype_t dt, char *name, int need_semi)
{
	ast_t *n = ast_new(AST_VARDECL);
	n->dtype = dt;
	n->name = name;
	if (tok.type == ASSIGN_OP)
	{
		next();
		n->a = parse_expr();
	}
	if (need_semi)
		expect(SEMI_OP);
	return n;
}

static ast_t *parse_assign_or_call_stmt(char *name)
{
	ast_t *n;
	if (tok.type == ASSIGN_OP)
	{
		n = ast_new(AST_ASSIGN);
		n->name = name;
		next();
		n->a = parse_expr();
		expect(SEMI_OP);
		return n;
	}
	if (tok.type == BRACEL_OP)
	{
		n = ast_new(AST_EXPRSTMT);
		n->a = ast_new(AST_CALL);
		n->a->name = name;
		next();
		n->a->a = parse_args();
		expect(BRACER_OP);
		expect(SEMI_OP);
		return n;
	}
	die(SYN_ERR);
	return NULL;
}

static ast_t *parse_if(void)
{
	ast_t *n = ast_new(AST_IF);
	next();
	expect(BRACEL_OP);
	n->a = parse_expr();
	expect(BRACER_OP);
	n->b = parse_stmt();
	if (tok.type == ELSE_KW)
	{
		next();
		n->c = parse_stmt();
	}
	return n;
}

static ast_t *parse_for(void)
{
	ast_t *n = ast_new(AST_FOR);
	next();
	expect(BRACEL_OP);
	if (is_type_kw(tok.type))
	{
		dtype_t dt = type_of_kw(tok.type);
		char *name;
		next();
		if (tok.type != IDENT_TK)
			die(SYN_ERR);
		name = tok.data.lit;
		tok.data.lit = NULL;
		next();
		n->a = parse_vardecl_body(dt, name, 0);
		expect(SEMI_OP);
	}
	else if (tok.type == IDENT_TK)
	{
		char *name = tok.data.lit;
		tok.data.lit = NULL;
		next();
		if (tok.type != ASSIGN_OP)
			die(SYN_ERR);
		n->a = ast_new(AST_ASSIGN);
		n->a->name = name;
		next();
		n->a->a = parse_expr();
		expect(SEMI_OP);
	}
	else
		expect(SEMI_OP);

	if (tok.type != SEMI_OP)
		n->b = parse_expr();
	expect(SEMI_OP);

	if (tok.type != BRACER_OP)
	{
		if (tok.type != IDENT_TK)
			die(SYN_ERR);
		n->c = ast_new(AST_ASSIGN);
		n->c->name = tok.data.lit;
		tok.data.lit = NULL;
		next();
		expect(ASSIGN_OP);
		n->c->a = parse_expr();
	}
	expect(BRACER_OP);
	n->d = parse_stmt();
	return n;
}

static ast_t *parse_while(void)
{
	ast_t *n = ast_new(AST_WHILE);
	next();
	expect(BRACEL_OP);
	n->a = parse_expr();
	expect(BRACER_OP);
	n->b = parse_stmt();
	return n;
}

static ast_t *parse_do(void)
{
	ast_t *n = ast_new(AST_DO);
	next();
	n->a = parse_stmt();
	expect(WHILE_KW);
	expect(BRACEL_OP);
	n->b = parse_expr();
	expect(BRACER_OP);
	expect(SEMI_OP);
	return n;
}

static ast_t *parse_cin(void)
{
	ast_t *n = ast_new(AST_CIN);
	next();
	expect(DBL_GRE_OP);
	if (tok.type != IDENT_TK)
		die(SYN_ERR);
	do
	{
		ast_t *id = ast_new(AST_IDENT);
		id->name = tok.data.lit;
		tok.data.lit = NULL;
		ast_append(&n->a, id);
		next();
		if (tok.type == DBL_GRE_OP)
			next();
		else
			break;
	} while (tok.type == IDENT_TK);
	expect(SEMI_OP);
	return n;
}

static ast_t *parse_cout(void)
{
	ast_t *n = ast_new(AST_COUT);
	next();
	expect(DBL_LESS_OP);
	ast_append(&n->a, parse_expr());
	while (tok.type == DBL_LESS_OP)
	{
		next();
		ast_append(&n->a, parse_expr());
	}
	expect(SEMI_OP);
	return n;
}

static ast_t *parse_block(void)
{
	ast_t *n = ast_new(AST_BLOCK);
	expect(BLOCKL_OP);
	while (tok.type != BLOCKR_OP)
	{
		if (tok.type == END_TK)
			die(SYN_ERR);
		ast_append(&n->a, parse_stmt());
	}
	expect(BLOCKR_OP);
	return n;
}

static ast_t *parse_stmt(void)
{
	if (tok.type == BLOCKL_OP)
		return parse_block();
	if (tok.type == SEMI_OP)
	{
		next();
		return ast_new(AST_BLOCK);
	}
	if (is_type_kw(tok.type))
	{
		dtype_t dt = type_of_kw(tok.type);
		char *name;
		next();
		if (tok.type != IDENT_TK)
			die(SYN_ERR);
		name = tok.data.lit;
		tok.data.lit = NULL;
		next();
		return parse_vardecl_body(dt, name, 1);
	}
	if (tok.type == IF_KW)
		return parse_if();
	if (tok.type == FOR_KW)
		return parse_for();
	if (tok.type == WHILE_KW)
		return parse_while();
	if (tok.type == DO_KW)
		return parse_do();
	if (tok.type == RETURN_KW)
	{
		ast_t *n = ast_new(AST_RETURN);
		next();
		if (tok.type != SEMI_OP)
			n->a = parse_expr();
		expect(SEMI_OP);
		return n;
	}
	if (tok.type == CIN_KW)
		return parse_cin();
	if (tok.type == COUT_KW)
		return parse_cout();
	if (tok.type == IDENT_TK)
	{
		char *name = tok.data.lit;
		tok.data.lit = NULL;
		next();
		return parse_assign_or_call_stmt(name);
	}
	die(SYN_ERR);
	return NULL;
}

static ast_t *parse_params(void)
{
	ast_t *list = NULL;
	if (tok.type == BRACER_OP)
		return NULL;
	for (;;)
	{
		ast_t *p;
		if (!is_type_kw(tok.type))
			die(SYN_ERR);
		p = ast_new(AST_PARAM);
		p->dtype = type_of_kw(tok.type);
		if (p->dtype == TY_AUTO)
			die(SEM_OTHER_ERR);
		next();
		if (tok.type != IDENT_TK)
			die(SYN_ERR);
		p->name = tok.data.lit;
		tok.data.lit = NULL;
		next();
		ast_append(&list, p);
		if (tok.type == COM_OP)
		{
			next();
			continue;
		}
		break;
	}
	return list;
}

static ast_t *parse_func(void)
{
	ast_t *n;
	if (!is_type_kw(tok.type))
		die(SYN_ERR);
	n = ast_new(AST_FUNC);
	n->dtype = type_of_kw(tok.type);
	if (n->dtype == TY_AUTO)
		die(SEM_OTHER_ERR);
	next();
	if (tok.type != IDENT_TK)
		die(SYN_ERR);
	n->name = tok.data.lit;
	tok.data.lit = NULL;
	next();
	expect(BRACEL_OP);
	n->a = parse_params();
	expect(BRACER_OP);
	if (tok.type == SEMI_OP)
	{
		next();
		return n;
	}
	n->b = parse_block();
	return n;
}

ast_t *parse_program(void)
{
	ast_t *prog = ast_new(AST_PROG);
	next();
	if (tok.type == END_TK)
		die(SYN_ERR);
	while (tok.type != END_TK)
		ast_append(&prog->a, parse_func());
	return prog;
}
