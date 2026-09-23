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

typedef enum flow_kind_e
{
	FLOW_NORMAL,
	FLOW_RETURN,
	FLOW_THROW
} flow_kind_t;

typedef struct exec_result_s
{
	flow_kind_t kind;
	value_t value;
} exec_result_t;

typedef struct frame_s
{
	htab_t *vars;
	struct frame_s *parent;
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

static void value_destroy(value_t *v)
{
	if (v->type == TY_STRING)
		free(v->s);
	*v = v_undef();
}

static value_t value_move(value_t *v)
{
	value_t moved = *v;
	*v = v_undef();
	return moved;
}

static exec_result_t result_normal(value_t value)
{
	exec_result_t result;
	result.kind = FLOW_NORMAL;
	result.value = value;
	return result;
}

static exec_result_t result_flow(flow_kind_t kind, value_t value)
{
	exec_result_t result;
	result.kind = kind;
	result.value = value;
	return result;
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

static exec_result_t eval_expr(ast_t *n);
static exec_result_t exec_stmt(ast_t *n);

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

static void destroy_values(value_t *values, int n)
{
	int i;
	for (i = 0; i < n; i++)
		value_destroy(&values[i]);
}

static exec_result_t exec_call(ast_t *call)
{
	func_t *fn;
	value_t args[32];
	int n = 0;
	ast_t *p;
	frame_t *fr;
	ast_t *param;
	exec_result_t result;

	if (call->name == NULL)
		die(SEM_ERR);
	fn = htab_get(functions, call->name);
	if (fn == NULL)
		die(SEM_ERR);

	for (p = call->a; p != NULL; p = p->next)
	{
		exec_result_t arg;
		if (n >= 32)
			die(SEM_OTHER_ERR);
		arg = eval_expr(p);
		if (arg.kind == FLOW_THROW)
		{
			destroy_values(args, n);
			return arg;
		}
		args[n++] = value_move(&arg.value);
	}

	if (fn->native)
	{
		value_t out = v_undef();
		int error = fn->nat(args, n, &out);
		destroy_values(args, n);
		if (error != 0)
			die(SEM_TYPE_ERR);
		return result_normal(out);
	}

	if (fn->def == NULL || fn->def->b == NULL)
		die(SEM_ERR);
	if (count_list(fn->def->a) != n)
		die(SEM_TYPE_ERR);

	{
		frame_t *caller = cur_frame;
		int i;
		fr = frame_push(NULL);
		param = fn->def->a;
		for (i = 0; i < n; i++)
		{
			value_t slot;
			memset(&slot, 0, sizeof(slot));
			slot.type = param->dtype;
			assign_into(&slot, args[i]);
			if (htab_put(fr->vars, param->name, val_new(slot)))
				die(SEM_ERR);
			value_destroy(&slot);
			param = param->next;
		}
		destroy_values(args, n);

		result = exec_stmt(fn->def->b);
		frame_pop(fr);
		cur_frame = caller;
	}

	if (result.kind == FLOW_RETURN)
		result.kind = FLOW_NORMAL;
	return result;
}

static exec_result_t eval_expr(ast_t *n)
{
	if (n == NULL)
		die(SYN_ERR);
	switch (n->kind)
	{
	case AST_INT:
		return result_normal(v_int(n->ival));
	case AST_DOUBLE:
		return result_normal(v_double(n->dval));
	case AST_STRING:
		return result_normal(v_string(xstrdup(n->sval ? n->sval : "")));
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
		return result_normal(copy);
	}
	case AST_UNOP:
	{
		exec_result_t operand = eval_expr(n->a);
		value_t out;
		if (operand.kind == FLOW_THROW)
			return operand;
		if (n->op == NOT_OP)
			out = v_int(!truthy(operand.value));
		else if (n->op == MINUS_OP)
		{
			if (!operand.value.init)
				die(RT_NO_INIT_ERR);
			if (operand.value.type == TY_INT)
				out = v_int(-operand.value.i);
			else if (operand.value.type == TY_DOUBLE)
				out = v_double(-operand.value.d);
			else
				die(SEM_TYPE_ERR);
		}
		else
		{
			die(SYN_ERR);
			out = v_undef();
		}
		value_destroy(&operand.value);
		return result_normal(out);
	}
	case AST_BINOP:
	{
		exec_result_t left = eval_expr(n->a);
		exec_result_t right;
		value_t out;
		int condition;
		if (left.kind == FLOW_THROW)
			return left;

		if (n->op == AND_OP)
		{
			condition = truthy(left.value);
			value_destroy(&left.value);
			if (!condition)
				return result_normal(v_int(0));
			right = eval_expr(n->b);
			if (right.kind == FLOW_THROW)
				return right;
			out = v_int(truthy(right.value));
			value_destroy(&right.value);
			return result_normal(out);
		}
		if (n->op == OR_OP)
		{
			condition = truthy(left.value);
			value_destroy(&left.value);
			if (condition)
				return result_normal(v_int(1));
			right = eval_expr(n->b);
			if (right.kind == FLOW_THROW)
				return right;
			out = v_int(truthy(right.value));
			value_destroy(&right.value);
			return result_normal(out);
		}

		right = eval_expr(n->b);
		if (right.kind == FLOW_THROW)
		{
			value_destroy(&left.value);
			return right;
		}
		out = num_bin(n->op, left.value, right.value);
		value_destroy(&left.value);
		value_destroy(&right.value);
		return result_normal(out);
	}
	case AST_CALL:
	{
		exec_result_t result = exec_call(n);
		if (result.kind == FLOW_THROW)
			return result;
		if (!result.value.init)
			die(RT_NO_INIT_ERR);
		return result;
	}
	default:
		die(SYN_ERR);
	}
	return result_normal(v_undef());
}

static exec_result_t exec_vardecl(ast_t *n)
{
	value_t slot;
	memset(&slot, 0, sizeof(slot));
	slot.type = n->dtype;
	if (n->a != NULL)
	{
		exec_result_t init = eval_expr(n->a);
		if (init.kind == FLOW_THROW)
			return init;
		assign_into(&slot, init.value);
		value_destroy(&init.value);
	}
	else if (n->dtype == TY_AUTO)
		die(AMB_TYPE_ERR);
	if (htab_get(cur_frame->vars, n->name) != NULL)
		die(SEM_ERR);
	htab_put(cur_frame->vars, n->name, val_new(slot));
	value_destroy(&slot);
	return result_normal(v_undef());
}

static exec_result_t exec_assign(ast_t *n)
{
	value_t *dst = lookup_var(cur_frame, n->name);
	exec_result_t value;
	if (dst == NULL)
		die(SEM_ERR);
	value = eval_expr(n->a);
	if (value.kind == FLOW_THROW)
		return value;
	assign_into(dst, value.value);
	value_destroy(&value.value);
	return result_normal(v_undef());
}

static exec_result_t exec_cin(ast_t *n)
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
	return result_normal(v_undef());
}

static exec_result_t exec_cout(ast_t *n)
{
	ast_t *e;
	for (e = n->a; e != NULL; e = e->next)
	{
		exec_result_t result = eval_expr(e);
		value_t *v;
		if (result.kind == FLOW_THROW)
			return result;
		v = &result.value;
		if (!v->init)
			die(RT_NO_INIT_ERR);
		if (v->type == TY_INT)
			printf("%d", v->i);
		else if (v->type == TY_DOUBLE)
			printf("%g", v->d);
		else if (v->type == TY_STRING)
			fputs(v->s ? v->s : "", stdout);
		else
			die(SEM_TYPE_ERR);
		value_destroy(v);
	}
	return result_normal(v_undef());
}

static exec_result_t exec_stmt(ast_t *n)
{
	if (n == NULL)
		return result_normal(v_undef());
	switch (n->kind)
	{
	case AST_BLOCK:
	{
		frame_t *inner = frame_push(cur_frame);
		ast_t *s;
		exec_result_t result = result_normal(v_undef());
		for (s = n->a; s != NULL && result.kind == FLOW_NORMAL; s = s->next)
			result = exec_stmt(s);
		frame_pop(inner);
		return result;
	}
	case AST_VARDECL:
		return exec_vardecl(n);
	case AST_ASSIGN:
		return exec_assign(n);
	case AST_CIN:
		return exec_cin(n);
	case AST_COUT:
		return exec_cout(n);
	case AST_RETURN:
	{
		if (n->a != NULL)
		{
			exec_result_t value = eval_expr(n->a);
			if (value.kind == FLOW_THROW)
				return value;
			return result_flow(FLOW_RETURN, value_move(&value.value));
		}
		return result_flow(FLOW_RETURN, v_undef());
	}
	case AST_THROW:
	{
		exec_result_t value = eval_expr(n->a);
		if (value.kind == FLOW_THROW)
			return value;
		if (!value.value.init)
			die(RT_NO_INIT_ERR);
		if (value.value.type != TY_STRING)
		{
			value_destroy(&value.value);
			die(SEM_TYPE_ERR);
		}
		return result_flow(FLOW_THROW, value_move(&value.value));
	}
	case AST_TRY:
	{
		exec_result_t result = exec_stmt(n->a);
		if (result.kind == FLOW_THROW)
		{
			frame_t *catch_frame = frame_push(cur_frame);
			value_t *caught = xmalloc(sizeof(*caught));
			*caught = value_move(&result.value);
			if (htab_put(catch_frame->vars, n->name, caught))
				die(SEM_ERR);
			result = exec_stmt(n->b);
			frame_pop(catch_frame);
		}
		return result;
	}
	case AST_IF:
	{
		exec_result_t condition = eval_expr(n->a);
		int take_then;
		if (condition.kind == FLOW_THROW)
			return condition;
		take_then = truthy(condition.value);
		value_destroy(&condition.value);
		return exec_stmt(take_then ? n->b : n->c);
	}
	case AST_WHILE:
		for (;;)
		{
			exec_result_t condition = eval_expr(n->a);
			exec_result_t body;
			int keep_going;
			if (condition.kind == FLOW_THROW)
				return condition;
			keep_going = truthy(condition.value);
			value_destroy(&condition.value);
			if (!keep_going)
				return result_normal(v_undef());
			body = exec_stmt(n->b);
			if (body.kind != FLOW_NORMAL)
				return body;
		}
	case AST_DO:
		for (;;)
		{
			exec_result_t body = exec_stmt(n->a);
			exec_result_t condition;
			int keep_going;
			if (body.kind != FLOW_NORMAL)
				return body;
			condition = eval_expr(n->b);
			if (condition.kind == FLOW_THROW)
				return condition;
			keep_going = truthy(condition.value);
			value_destroy(&condition.value);
			if (!keep_going)
				return result_normal(v_undef());
		}
	case AST_FOR:
	{
		frame_t *inner = frame_push(cur_frame);
		exec_result_t result = result_normal(v_undef());
		if (n->a != NULL)
			result = exec_stmt(n->a);
		while (result.kind == FLOW_NORMAL)
		{
			if (n->b != NULL)
			{
				exec_result_t condition = eval_expr(n->b);
				int keep_going;
				if (condition.kind == FLOW_THROW)
				{
					result = condition;
					break;
				}
				keep_going = truthy(condition.value);
				value_destroy(&condition.value);
				if (!keep_going)
					break;
			}
			result = exec_stmt(n->d);
			if (result.kind != FLOW_NORMAL)
				break;
			if (n->c != NULL)
				result = exec_stmt(n->c);
		}
		frame_pop(inner);
		return result;
	}
	case AST_EXPRSTMT:
		if (n->a != NULL)
		{
			exec_result_t result = n->a->kind == AST_CALL
				? exec_call(n->a) : eval_expr(n->a);
			if (result.kind == FLOW_THROW)
				return result;
			value_destroy(&result.value);
		}
		return result_normal(v_undef());
	default:
		die(SYN_ERR);
	}
	return result_normal(v_undef());
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
	ast_t call;
	exec_result_t result;
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
	result = exec_call(&call);
	if (result.kind == FLOW_THROW)
	{
		value_destroy(&result.value);
		die(RT_OTHER_ERR);
	}
	value_destroy(&result.value);
}
