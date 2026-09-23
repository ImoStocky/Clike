#include "interp.h"
#include "ial.h"
#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct value_s
{
	dtype_t type;
	int init;
	int i;
	double d;
	char *s;
} value_t;

typedef struct frame_s
{
	htab_t *vars;
	struct frame_s *parent;
	int returned;
	value_t ret;
} frame_t;

typedef struct func_s
{
	int native;
	int (*nat)(value_t *args, int n, value_t *out);
	ast_t *def;
} func_t;

static htab_t *functions;
static frame_t *cur_frame;

static value_t v_undef(void)
{
	value_t v;
	memset(&v, 0, sizeof(v));
	v.type = TY_VOID;
	return v;
}

static value_t v_int(int x)
{
	value_t v = v_undef();
	v.type = TY_INT;
	v.init = 1;
	v.i = x;
	return v;
}

static value_t v_double(double x)
{
	value_t v = v_undef();
	v.type = TY_DOUBLE;
	v.init = 1;
	v.d = x;
	return v;
}

static value_t v_string(char *s)
{
	value_t v = v_undef();
	v.type = TY_STRING;
	v.init = 1;
	v.s = s;
	return v;
}

static value_t *val_new(value_t src)
{
	value_t *p = xmalloc(sizeof(*p));
	*p = src;
	if (src.type == TY_STRING && src.s != NULL)
		p->s = xstrdup(src.s);
	return p;
}

static void val_free(void *p)
{
	value_t *v = p;
	if (v == NULL)
		return;
	if (v->type == TY_STRING)
		free(v->s);
	free(v);
}

static frame_t *frame_push(frame_t *parent)
{
	frame_t *f = xmalloc(sizeof(*f));
	memset(f, 0, sizeof(*f));
	f->vars = htab_init(32);
	f->parent = parent;
	cur_frame = f;
	return f;
}

static void frame_pop(frame_t *f)
{
	cur_frame = f->parent;
	htab_free(f->vars, val_free);
	free(f);
}

static value_t *lookup_var(frame_t *f, const char *name)
{
	while (f != NULL)
	{
		value_t *v = htab_get(f->vars, name);
		if (v != NULL)
			return v;
		f = f->parent;
	}
	return NULL;
}

static int truthy(value_t v)
{
	if (!v.init)
		die(RT_NO_INIT_ERR);
	if (v.type == TY_INT)
		return v.i != 0;
	if (v.type == TY_DOUBLE)
		return v.d != 0.0;
	if (v.type == TY_STRING)
		return v.s != NULL && v.s[0] != '\0';
	die(SEM_TYPE_ERR);
	return 0;
}

static value_t eval_expr(ast_t *n);
static void exec_stmt(ast_t *n);

static void assign_into(value_t *dst, value_t src)
{
	if (!src.init)
		die(RT_NO_INIT_ERR);
	if (dst->type == TY_AUTO)
		dst->type = src.type;
	if (dst->type == TY_INT)
	{
		if (src.type == TY_INT)
			dst->i = src.i;
		else if (src.type == TY_DOUBLE)
			dst->i = (int)src.d;
		else
			die(SEM_TYPE_ERR);
		free(dst->s);
		dst->s = NULL;
	}
	else if (dst->type == TY_DOUBLE)
	{
		if (src.type == TY_INT)
			dst->d = (double)src.i;
		else if (src.type == TY_DOUBLE)
			dst->d = src.d;
		else
			die(SEM_TYPE_ERR);
		free(dst->s);
		dst->s = NULL;
	}
	else if (dst->type == TY_STRING)
	{
		if (src.type != TY_STRING)
			die(SEM_TYPE_ERR);
		free(dst->s);
		dst->s = xstrdup(src.s ? src.s : "");
	}
	else
		die(SEM_TYPE_ERR);
	dst->init = 1;
}

static value_t num_bin(token_kind_t op, value_t a, value_t b)
{
	int use_d;
	double da, db;
	int ia, ib;
	if (!a.init || !b.init)
		die(RT_NO_INIT_ERR);
	if (a.type == TY_STRING || b.type == TY_STRING)
	{
		if (op == PLUS_OP && a.type == TY_STRING && b.type == TY_STRING)
		{
			size_t n = strlen(a.s ? a.s : "") + strlen(b.s ? b.s : "");
			char *s = xmalloc(n + 1);
			strcpy(s, a.s ? a.s : "");
			strcat(s, b.s ? b.s : "");
			return v_string(s);
		}
		if (op == EQ_OP && a.type == TY_STRING && b.type == TY_STRING)
			return v_int(strcmp(a.s ? a.s : "", b.s ? b.s : "") == 0);
		if (op == NEQ_OP && a.type == TY_STRING && b.type == TY_STRING)
			return v_int(strcmp(a.s ? a.s : "", b.s ? b.s : "") != 0);
		if ((op == LESS_OP || op == GREAT_OP || op == LEE_OP || op == GRE_OP)
			&& a.type == TY_STRING && b.type == TY_STRING)
		{
			int c = strcmp(a.s ? a.s : "", b.s ? b.s : "");
			if (op == LESS_OP) return v_int(c < 0);
			if (op == GREAT_OP) return v_int(c > 0);
			if (op == LEE_OP) return v_int(c <= 0);
			return v_int(c >= 0);
		}
		die(SEM_TYPE_ERR);
	}
	if ((a.type != TY_INT && a.type != TY_DOUBLE) ||
		(b.type != TY_INT && b.type != TY_DOUBLE))
		die(SEM_TYPE_ERR);

	use_d = (a.type == TY_DOUBLE || b.type == TY_DOUBLE);
	da = (a.type == TY_DOUBLE) ? a.d : (double)a.i;
	db = (b.type == TY_DOUBLE) ? b.d : (double)b.i;
	ia = a.i;
	ib = b.i;

	if (op == EQ_OP) return v_int(use_d ? da == db : ia == ib);
	if (op == NEQ_OP) return v_int(use_d ? da != db : ia != ib);
	if (op == LESS_OP) return v_int(use_d ? da < db : ia < ib);
	if (op == GREAT_OP) return v_int(use_d ? da > db : ia > ib);
	if (op == LEE_OP) return v_int(use_d ? da <= db : ia <= ib);
	if (op == GRE_OP) return v_int(use_d ? da >= db : ia >= ib);
	if (op == AND_OP) return v_int(truthy(a) && truthy(b));
	if (op == OR_OP) return v_int(truthy(a) || truthy(b));

	if (op == SLASH_OP)
	{
		if (use_d)
		{
			if (db == 0.0)
				die(RT_ZDIV_ERR);
			return v_double(da / db);
		}
		if (ib == 0)
			die(RT_ZDIV_ERR);
		return v_int(ia / ib);
	}
	if (op == PERCENT_OP)
	{
		if (use_d)
			die(SEM_TYPE_ERR);
		if (ib == 0)
			die(RT_ZDIV_ERR);
		return v_int(ia % ib);
	}
	if (op == PLUS_OP)
	{
		if (use_d) return v_double(da + db);
		return v_int(ia + ib);
	}
	if (op == MINUS_OP)
	{
		if (use_d) return v_double(da - db);
		return v_int(ia - ib);
	}
	if (op == ASTERISK_OP)
	{
		if (use_d) return v_double(da * db);
		return v_int(ia * ib);
	}
	die(SEM_TYPE_ERR);
	return v_undef();
}

static int count_list(ast_t *n)
{
	int c = 0;
	while (n != NULL)
	{
		c++;
		n = n->next;
	}
	return c;
}

static void exec_call(ast_t *call, value_t *out)
{
	func_t *fn;
	value_t args[32];
	int n = 0;
	ast_t *p;
	frame_t *fr;
	ast_t *param;
	if (call->name == NULL)
		die(SEM_ERR);
	fn = htab_get(functions, call->name);
	if (fn == NULL)
		die(SEM_ERR);
	for (p = call->a; p != NULL; p = p->next)
	{
		if (n >= 32)
			die(SEM_OTHER_ERR);
		args[n++] = eval_expr(p);
	}
	if (fn->native)
	{
		if (fn->nat(args, n, out) != 0)
			die(SEM_TYPE_ERR);
		return;
	}
	if (fn->def == NULL || fn->def->b == NULL)
		die(SEM_ERR);
	if (count_list(fn->def->a) != n)
		die(SEM_TYPE_ERR);
	{
		frame_t *caller = cur_frame;
		fr = frame_push(NULL);
		param = fn->def->a;
		{
			int i;
			for (i = 0; i < n; i++)
			{
				value_t slot;
				memset(&slot, 0, sizeof(slot));
				slot.type = param->dtype;
				assign_into(&slot, args[i]);
				if (htab_put(fr->vars, param->name, val_new(slot)))
					die(SEM_ERR);
				if (slot.type == TY_STRING)
					free(slot.s);
				param = param->next;
			}
		}
		exec_stmt(fn->def->b);
		if (fr->returned)
			*out = fr->ret;
		else
			*out = v_undef();
		if (fn->def->dtype != TY_VOID && !out->init && fn->def->dtype != TY_AUTO)
		{
			/* missing return is allowed until value is used */
		}
		frame_pop(fr);
		cur_frame = caller;
	}
}

static value_t eval_expr(ast_t *n)
{
	if (n == NULL)
		die(SYN_ERR);
	switch (n->kind)
	{
	case AST_INT:
		return v_int(n->ival);
	case AST_DOUBLE:
		return v_double(n->dval);
	case AST_STRING:
		return v_string(xstrdup(n->sval ? n->sval : ""));
	case AST_IDENT:
	{
		value_t *v = lookup_var(cur_frame, n->name);
		value_t copy;
		if (v == NULL)
			die(SEM_ERR);
		if (!v->init)
			die(RT_NO_INIT_ERR);
		copy = *v;
		if (v->type == TY_STRING)
			copy.s = xstrdup(v->s ? v->s : "");
		return copy;
	}
	case AST_UNOP:
	{
		value_t a = eval_expr(n->a);
		if (n->op == NOT_OP)
			return v_int(!truthy(a));
		if (n->op == MINUS_OP)
		{
			if (!a.init)
				die(RT_NO_INIT_ERR);
			if (a.type == TY_INT)
				return v_int(-a.i);
			if (a.type == TY_DOUBLE)
				return v_double(-a.d);
			die(SEM_TYPE_ERR);
		}
		die(SYN_ERR);
		break;
	}
	case AST_BINOP:
	{
		value_t a, b;
		if (n->op == AND_OP)
		{
			a = eval_expr(n->a);
			if (!truthy(a))
				return v_int(0);
			b = eval_expr(n->b);
			return v_int(truthy(b));
		}
		if (n->op == OR_OP)
		{
			a = eval_expr(n->a);
			if (truthy(a))
				return v_int(1);
			b = eval_expr(n->b);
			return v_int(truthy(b));
		}
		a = eval_expr(n->a);
		b = eval_expr(n->b);
		return num_bin(n->op, a, b);
	}
	case AST_CALL:
	{
		value_t out = v_undef();
		exec_call(n, &out);
		if (!out.init)
			die(RT_NO_INIT_ERR);
		return out;
	}
	default:
		die(SYN_ERR);
	}
	return v_undef();
}

static void exec_vardecl(ast_t *n)
{
	value_t slot;
	memset(&slot, 0, sizeof(slot));
	slot.type = n->dtype;
	if (n->a != NULL)
		assign_into(&slot, eval_expr(n->a));
	else if (n->dtype == TY_AUTO)
		die(AMB_TYPE_ERR);
	if (htab_get(cur_frame->vars, n->name) != NULL)
		die(SEM_ERR);
	htab_put(cur_frame->vars, n->name, val_new(slot));
	if (slot.type == TY_STRING)
		free(slot.s);
}

static void exec_assign(ast_t *n)
{
	value_t *dst = lookup_var(cur_frame, n->name);
	if (dst == NULL)
		die(SEM_ERR);
	assign_into(dst, eval_expr(n->a));
}

static void exec_cin(ast_t *n)
{
	ast_t *id;
	for (id = n->a; id != NULL; id = id->next)
	{
		value_t *dst = lookup_var(cur_frame, id->name);
		if (dst == NULL)
			die(SEM_ERR);
		if (dst->type == TY_AUTO)
			die(AMB_TYPE_ERR);
		if (dst->type == TY_INT)
		{
			int x;
			if (scanf("%d", &x) != 1)
				die(RT_NUM_ERR);
			dst->i = x;
			dst->init = 1;
		}
		else if (dst->type == TY_DOUBLE)
		{
			double x;
			if (scanf("%lf", &x) != 1)
				die(RT_NUM_ERR);
			dst->d = x;
			dst->init = 1;
		}
		else if (dst->type == TY_STRING)
		{
			char tmp[4096];
			if (scanf("%4095s", tmp) != 1)
				die(RT_NUM_ERR);
			free(dst->s);
			dst->s = xstrdup(tmp);
			dst->init = 1;
		}
		else
			die(SEM_TYPE_ERR);
	}
}

static void exec_cout(ast_t *n)
{
	ast_t *e;
	for (e = n->a; e != NULL; e = e->next)
	{
		value_t v = eval_expr(e);
		if (!v.init)
			die(RT_NO_INIT_ERR);
		if (v.type == TY_INT)
			printf("%d", v.i);
		else if (v.type == TY_DOUBLE)
			printf("%g", v.d);
		else if (v.type == TY_STRING)
			fputs(v.s ? v.s : "", stdout);
		else
			die(SEM_TYPE_ERR);
		if (v.type == TY_STRING)
			free(v.s);
	}
}

static void exec_stmt(ast_t *n)
{
	if (n == NULL || (cur_frame && cur_frame->returned))
		return;
	switch (n->kind)
	{
	case AST_BLOCK:
	{
		frame_t *inner = frame_push(cur_frame);
		ast_t *s;
		for (s = n->a; s != NULL && !inner->returned; s = s->next)
			exec_stmt(s);
		if (inner->returned)
		{
			cur_frame->parent->returned = 1;
			cur_frame->parent->ret = inner->ret;
		}
		frame_pop(inner);
		break;
	}
	case AST_VARDECL:
		exec_vardecl(n);
		break;
	case AST_ASSIGN:
		exec_assign(n);
		break;
	case AST_CIN:
		exec_cin(n);
		break;
	case AST_COUT:
		exec_cout(n);
		break;
	case AST_RETURN:
	{
		if (n->a != NULL)
			cur_frame->ret = eval_expr(n->a);
		else
			cur_frame->ret = v_undef();
		cur_frame->returned = 1;
		break;
	}
	case AST_IF:
		if (truthy(eval_expr(n->a)))
			exec_stmt(n->b);
		else
			exec_stmt(n->c);
		break;
	case AST_WHILE:
		while (!cur_frame->returned && truthy(eval_expr(n->a)))
			exec_stmt(n->b);
		break;
	case AST_DO:
		do
			exec_stmt(n->a);
		while (!cur_frame->returned && truthy(eval_expr(n->b)));
		break;
	case AST_FOR:
	{
		frame_t *inner = frame_push(cur_frame);
		if (n->a)
			exec_stmt(n->a);
		while (!inner->returned && (n->b == NULL || truthy(eval_expr(n->b))))
		{
			exec_stmt(n->d);
			if (inner->returned)
				break;
			if (n->c)
				exec_stmt(n->c);
		}
		if (inner->returned)
		{
			cur_frame->parent->returned = 1;
			cur_frame->parent->ret = inner->ret;
		}
		frame_pop(inner);
		break;
	}
	case AST_EXPRSTMT:
		if (n->a != NULL)
		{
			value_t v = v_undef();
			if (n->a->kind == AST_CALL)
				exec_call(n->a, &v);
			else
				v = eval_expr(n->a);
			if (v.type == TY_STRING)
				free(v.s);
		}
		break;
	default:
		die(SYN_ERR);
	}
}

static int nat_length(value_t *args, int n, value_t *out)
{
	if (n != 1 || args[0].type != TY_STRING || !args[0].init)
		return 1;
	*out = v_int((int)strlen(args[0].s ? args[0].s : ""));
	return 0;
}

static int nat_concat(value_t *args, int n, value_t *out)
{
	size_t len;
	char *s;
	if (n != 2 || args[0].type != TY_STRING || args[1].type != TY_STRING)
		return 1;
	if (!args[0].init || !args[1].init)
		die(RT_NO_INIT_ERR);
	len = strlen(args[0].s ? args[0].s : "") + strlen(args[1].s ? args[1].s : "");
	s = xmalloc(len + 1);
	strcpy(s, args[0].s ? args[0].s : "");
	strcat(s, args[1].s ? args[1].s : "");
	*out = v_string(s);
	return 0;
}

static int nat_substr(value_t *args, int n, value_t *out)
{
	int i, m, len;
	char *s;
	const char *src;
	if (n != 3 || args[0].type != TY_STRING || args[1].type != TY_INT || args[2].type != TY_INT)
		return 1;
	if (!args[0].init || !args[1].init || !args[2].init)
		die(RT_NO_INIT_ERR);
	src = args[0].s ? args[0].s : "";
	len = (int)strlen(src);
	i = args[1].i;
	m = args[2].i;
	if (i < 0 || m < 0)
		die(RT_OTHER_ERR);
	if (i > len)
		i = len;
	if (i + m > len)
		m = len - i;
	s = xmalloc((size_t)m + 1);
	memcpy(s, src + i, (size_t)m);
	s[m] = '\0';
	*out = v_string(s);
	return 0;
}

static int nat_find(value_t *args, int n, value_t *out)
{
	if (n != 2 || args[0].type != TY_STRING || args[1].type != TY_STRING)
		return 1;
	if (!args[0].init || !args[1].init)
		die(RT_NO_INIT_ERR);
	*out = v_int(kmp_find(args[0].s ? args[0].s : "", args[1].s ? args[1].s : ""));
	return 0;
}

static int nat_sort(value_t *args, int n, value_t *out)
{
	char *s;
	if (n != 1 || args[0].type != TY_STRING || !args[0].init)
		return 1;
	s = xstrdup(args[0].s ? args[0].s : "");
	heap_sort_chars(s);
	*out = v_string(s);
	return 0;
}

static void add_native(const char *name, int (*fn)(value_t *, int, value_t *))
{
	func_t *f = xmalloc(sizeof(*f));
	memset(f, 0, sizeof(*f));
	f->native = 1;
	f->nat = fn;
	htab_put(functions, name, f);
}

static void register_func(ast_t *fn)
{
	func_t *old = htab_get(functions, fn->name);
	if (old != NULL)
	{
		if (old->native)
			die(SEM_ERR);
		if (old->def != NULL && old->def->b != NULL && fn->b != NULL)
			die(SEM_ERR);
		if (fn->b != NULL)
			old->def = fn;
		return;
	}
	{
		func_t *f = xmalloc(sizeof(*f));
		memset(f, 0, sizeof(*f));
		f->def = fn;
		htab_put(functions, fn->name, f);
	}
}

void interpret(ast_t *prog)
{
	ast_t *f;
	func_t *mainfn;
	value_t dummy;
	ast_t call;
	functions = htab_init(64);
	add_native("length", nat_length);
	add_native("concat", nat_concat);
	add_native("substr", nat_substr);
	add_native("find", nat_find);
	add_native("sort", nat_sort);

	for (f = prog->a; f != NULL; f = f->next)
		register_func(f);

	mainfn = htab_get(functions, "main");
	if (mainfn == NULL || mainfn->def == NULL || mainfn->def->b == NULL)
		die(SEM_ERR);
	if (mainfn->def->a != NULL)
		die(SEM_TYPE_ERR);

	memset(&call, 0, sizeof(call));
	call.kind = AST_CALL;
	call.name = "main";
	exec_call(&call, &dummy);
	if (dummy.type == TY_STRING)
		free(dummy.s);
}
