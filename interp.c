#include "interp.h"
#include "ial.h"
#include "types.h"

#include <stdint.h>
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
	size_t slot;
} exec_result_t;

typedef struct frame_s
{
	htab_t *vars;
	struct frame_s *parent;
	size_t base;
} frame_t;

typedef struct func_s
{
	int native;
	int (*nat)(value_t *args, int n, value_t *out);
	ast_t *def;
} func_t;

static htab_t *functions;
static frame_t *cur_frame;
static value_t *ostack;
static size_t otop;
static size_t ocap;

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

static exec_result_t result_none(void)
{
	exec_result_t result;
	result.kind = FLOW_NORMAL;
	result.slot = (size_t)-1;
	return result;
}

static size_t ostack_push(value_t value)
{
	if (otop == ocap)
	{
		size_t ncap = ocap == 0 ? 32 : ocap * 2;
		value_t *grown = realloc(ostack, ncap * sizeof(*ostack));
		if (grown == NULL)
			die(INTER_ERR);
		ostack = grown;
		ocap = ncap;
	}
	ostack[otop] = value;
	return otop++;
}

static value_t ostack_take(size_t slot)
{
	value_t value = ostack[slot];
	ostack[slot] = v_undef();
	return value;
}

static void ostack_rewind(size_t marker)
{
	while (otop > marker)
	{
		otop--;
		value_destroy(&ostack[otop]);
	}
}

static exec_result_t result_value(flow_kind_t kind, value_t value)
{
	exec_result_t result;
	result.kind = kind;
	result.slot = ostack_push(value);
	return result;
}

static void *slot_ptr(size_t slot)
{
	return (void *)(uintptr_t)(slot + 1);
}

static size_t ptr_slot(void *ptr)
{
	return (size_t)(uintptr_t)ptr - 1;
}

static frame_t *frame_push(frame_t *parent, size_t base)
{
	frame_t *f = xmalloc(sizeof(*f));
	memset(f, 0, sizeof(*f));
	f->vars = htab_init(32);
	f->parent = parent;
	f->base = base;
	cur_frame = f;
	return f;
}

static void frame_pop(frame_t *f)
{
	cur_frame = f->parent;
	htab_free(f->vars, NULL);
	free(f);
}

static exec_result_t frame_leave(frame_t *frame, exec_result_t result)
{
	if (result.kind != FLOW_NORMAL)
	{
		value_t saved = ostack_take(result.slot);
		ostack_rewind(frame->base);
		result.slot = ostack_push(saved);
	}
	else
		ostack_rewind(frame->base);
	frame_pop(frame);
	return result;
}

static int lookup_slot(frame_t *frame, const char *name, size_t *slot)
{
	while (frame != NULL)
	{
		void *found = htab_get(frame->vars, name);
		if (found != NULL)
		{
			*slot = ptr_slot(found);
			return 1;
		}
		frame = frame->parent;
	}
	return 0;
}

static void bind_slot(frame_t *frame, const char *name, size_t slot)
{
	if (htab_get(frame->vars, name) != NULL ||
		htab_put(frame->vars, name, slot_ptr(slot)))
		die(SEM_ERR);
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

static exec_result_t exec_call(ast_t *call)
{
	func_t *fn;
	ast_t *param;
	ast_t *arg;
	frame_t *caller;
	frame_t *frame;
	exec_result_t result;
	size_t arg_base = otop;
	int n = 0;
	int i;

	if (call->name == NULL)
		die(SEM_ERR);
	fn = htab_get(functions, call->name);
	if (fn == NULL)
		die(SEM_ERR);

	for (arg = call->a; arg != NULL; arg = arg->next)
	{
		result = eval_expr(arg);
		if (result.kind == FLOW_THROW)
		{
			value_t thrown = ostack_take(result.slot);
			ostack_rewind(arg_base);
			return result_value(FLOW_THROW, thrown);
		}
		n++;
		if (n > 32)
			die(SEM_OTHER_ERR);
	}

	if (fn->native)
	{
		value_t out = v_undef();
		int error = fn->nat(ostack + arg_base, n, &out);
		ostack_rewind(arg_base);
		if (error != 0)
			die(SEM_TYPE_ERR);
		return result_value(FLOW_NORMAL, out);
	}
	if (fn->def == NULL || fn->def->b == NULL)
		die(SEM_ERR);
	if (count_list(fn->def->a) != n)
		die(SEM_TYPE_ERR);

	param = fn->def->a;
	for (i = 0; i < n; i++)
	{
		value_t slot = v_undef();
		slot.type = param->dtype;
		assign_into(&slot, ostack[arg_base + (size_t)i]);
		value_destroy(&ostack[arg_base + (size_t)i]);
		ostack[arg_base + (size_t)i] = slot;
		param = param->next;
	}

	caller = cur_frame;
	frame = frame_push(NULL, arg_base);
	param = fn->def->a;
	for (i = 0; i < n; i++)
	{
		bind_slot(frame, param->name, arg_base + (size_t)i);
		param = param->next;
	}
	result = frame_leave(frame, exec_stmt(fn->def->b));
	cur_frame = caller;
	if (result.kind == FLOW_RETURN)
		result.kind = FLOW_NORMAL;
	else if (result.kind == FLOW_NORMAL)
		result = result_value(FLOW_NORMAL, v_undef());
	return result;
}

static exec_result_t eval_expr(ast_t *n)
{
	size_t marker;
	exec_result_t left;
	exec_result_t right;
	value_t out;
	int condition;

	if (n == NULL)
		die(SYN_ERR);
	switch (n->kind)
	{
	case AST_INT:
		return result_value(FLOW_NORMAL, v_int(n->ival));
	case AST_DOUBLE:
		return result_value(FLOW_NORMAL, v_double(n->dval));
	case AST_STRING:
		return result_value(FLOW_NORMAL, v_string(xstrdup(n->sval ? n->sval : "")));
	case AST_IDENT:
	{
		size_t slot;
		value_t copy;
		if (!lookup_slot(cur_frame, n->name, &slot))
			die(SEM_ERR);
		if (!ostack[slot].init)
			die(RT_NO_INIT_ERR);
		copy = ostack[slot];
		if (copy.type == TY_STRING)
			copy.s = xstrdup(copy.s ? copy.s : "");
		return result_value(FLOW_NORMAL, copy);
	}
	case AST_UNOP:
		marker = otop;
		left = eval_expr(n->a);
		if (left.kind == FLOW_THROW)
			return left;
		if (n->op == NOT_OP)
			out = v_int(!truthy(ostack[left.slot]));
		else if (n->op == MINUS_OP)
		{
			if (!ostack[left.slot].init)
				die(RT_NO_INIT_ERR);
			if (ostack[left.slot].type == TY_INT)
				out = v_int(-ostack[left.slot].i);
			else if (ostack[left.slot].type == TY_DOUBLE)
				out = v_double(-ostack[left.slot].d);
			else
				die(SEM_TYPE_ERR);
		}
		else
		{
			die(SYN_ERR);
			out = v_undef();
		}
		ostack_rewind(marker);
		return result_value(FLOW_NORMAL, out);
	case AST_BINOP:
		marker = otop;
		left = eval_expr(n->a);
		if (left.kind == FLOW_THROW)
			return left;
		if (n->op == AND_OP || n->op == OR_OP)
		{
			condition = truthy(ostack[left.slot]);
			ostack_rewind(marker);
			if ((n->op == AND_OP && !condition) || (n->op == OR_OP && condition))
				return result_value(FLOW_NORMAL, v_int(n->op == OR_OP));
			right = eval_expr(n->b);
			if (right.kind == FLOW_THROW)
				return right;
			condition = truthy(ostack[right.slot]);
			ostack_rewind(right.slot);
			return result_value(FLOW_NORMAL, v_int(condition));
		}
		right = eval_expr(n->b);
		if (right.kind == FLOW_THROW)
		{
			value_t thrown = ostack_take(right.slot);
			ostack_rewind(marker);
			return result_value(FLOW_THROW, thrown);
		}
		out = num_bin(n->op, ostack[left.slot], ostack[right.slot]);
		ostack_rewind(marker);
		return result_value(FLOW_NORMAL, out);
	case AST_CALL:
		left = exec_call(n);
		if (left.kind == FLOW_THROW)
			return left;
		if (!ostack[left.slot].init)
			die(RT_NO_INIT_ERR);
		return left;
	default:
		die(SYN_ERR);
	}
	return result_none();
}

static exec_result_t exec_vardecl(ast_t *n)
{
	value_t slot;
	size_t index;
	exec_result_t init;

	if (htab_get(cur_frame->vars, n->name) != NULL)
		die(SEM_ERR);
	memset(&slot, 0, sizeof(slot));
	slot.type = n->dtype;
	index = ostack_push(slot);
	bind_slot(cur_frame, n->name, index);
	if (n->a != NULL)
	{
		init = eval_expr(n->a);
		if (init.kind == FLOW_THROW)
			return init;
		assign_into(&ostack[index], ostack[init.slot]);
		ostack_rewind(init.slot);
	}
	else if (n->dtype == TY_AUTO)
		die(AMB_TYPE_ERR);
	return result_none();
}

static exec_result_t exec_assign(ast_t *n)
{
	size_t dst;
	exec_result_t value;

	if (!lookup_slot(cur_frame, n->name, &dst))
		die(SEM_ERR);
	value = eval_expr(n->a);
	if (value.kind == FLOW_THROW)
		return value;
	assign_into(&ostack[dst], ostack[value.slot]);
	ostack_rewind(value.slot);
	return result_none();
}

static exec_result_t exec_cin(ast_t *n)
{
	ast_t *id;
	for (id = n->a; id != NULL; id = id->next)
	{
		size_t dst;
		if (!lookup_slot(cur_frame, id->name, &dst))
			die(SEM_ERR);
		if (ostack[dst].type == TY_AUTO)
			die(AMB_TYPE_ERR);
		if (ostack[dst].type == TY_INT)
		{
			int x;
			if (scanf("%d", &x) != 1)
				die(RT_NUM_ERR);
			ostack[dst].i = x;
			ostack[dst].init = 1;
		}
		else if (ostack[dst].type == TY_DOUBLE)
		{
			double x;
			if (scanf("%lf", &x) != 1)
				die(RT_NUM_ERR);
			ostack[dst].d = x;
			ostack[dst].init = 1;
		}
		else if (ostack[dst].type == TY_STRING)
		{
			char tmp[4096];
			if (scanf("%4095s", tmp) != 1)
				die(RT_NUM_ERR);
			free(ostack[dst].s);
			ostack[dst].s = xstrdup(tmp);
			ostack[dst].init = 1;
		}
		else
			die(SEM_TYPE_ERR);
	}
	return result_none();
}

static exec_result_t exec_cout(ast_t *n)
{
	ast_t *e;
	for (e = n->a; e != NULL; e = e->next)
	{
		exec_result_t result = eval_expr(e);
		value_t *value;
		if (result.kind == FLOW_THROW)
			return result;
		value = &ostack[result.slot];
		if (!value->init)
			die(RT_NO_INIT_ERR);
		if (value->type == TY_INT)
			printf("%d", value->i);
		else if (value->type == TY_DOUBLE)
			printf("%g", value->d);
		else if (value->type == TY_STRING)
			fputs(value->s ? value->s : "", stdout);
		else
			die(SEM_TYPE_ERR);
		ostack_rewind(result.slot);
	}
	return result_none();
}

static exec_result_t exec_stmt(ast_t *n)
{
	exec_result_t result;
	if (n == NULL)
		return result_none();
	switch (n->kind)
	{
	case AST_BLOCK:
	{
		frame_t *inner = frame_push(cur_frame, otop);
		ast_t *statement;
		result = result_none();
		for (statement = n->a; statement != NULL && result.kind == FLOW_NORMAL; statement = statement->next)
			result = exec_stmt(statement);
		return frame_leave(inner, result);
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
		if (n->a == NULL)
			return result_value(FLOW_RETURN, v_undef());
		result = eval_expr(n->a);
		if (result.kind == FLOW_NORMAL)
			result.kind = FLOW_RETURN;
		return result;
	case AST_THROW:
		result = eval_expr(n->a);
		if (result.kind == FLOW_THROW)
			return result;
		if (!ostack[result.slot].init)
			die(RT_NO_INIT_ERR);
		if (ostack[result.slot].type != TY_STRING)
		{
			ostack_rewind(result.slot);
			die(SEM_TYPE_ERR);
		}
		result.kind = FLOW_THROW;
		return result;
	case AST_TRY:
		result = exec_stmt(n->a);
		if (result.kind == FLOW_THROW)
		{
			value_t thrown = ostack_take(result.slot);
			frame_t *catch_frame;
			size_t base;
			ostack_rewind(result.slot);
			base = otop;
			catch_frame = frame_push(cur_frame, base);
			bind_slot(catch_frame, n->name, ostack_push(thrown));
			result = frame_leave(catch_frame, exec_stmt(n->b));
		}
		return result;
	case AST_IF:
	{
		int take_then;
		result = eval_expr(n->a);
		if (result.kind == FLOW_THROW)
			return result;
		take_then = truthy(ostack[result.slot]);
		ostack_rewind(result.slot);
		return exec_stmt(take_then ? n->b : n->c);
	}
	case AST_WHILE:
		for (;;)
		{
			int keep_going;
			result = eval_expr(n->a);
			if (result.kind == FLOW_THROW)
				return result;
			keep_going = truthy(ostack[result.slot]);
			ostack_rewind(result.slot);
			if (!keep_going)
				return result_none();
			result = exec_stmt(n->b);
			if (result.kind != FLOW_NORMAL)
				return result;
		}
	case AST_DO:
		for (;;)
		{
			int keep_going;
			result = exec_stmt(n->a);
			if (result.kind != FLOW_NORMAL)
				return result;
			result = eval_expr(n->b);
			if (result.kind == FLOW_THROW)
				return result;
			keep_going = truthy(ostack[result.slot]);
			ostack_rewind(result.slot);
			if (!keep_going)
				return result_none();
		}
	case AST_FOR:
	{
		frame_t *inner = frame_push(cur_frame, otop);
		result = result_none();
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
				keep_going = truthy(ostack[condition.slot]);
				ostack_rewind(condition.slot);
				if (!keep_going)
					break;
			}
			result = exec_stmt(n->d);
			if (result.kind != FLOW_NORMAL)
				break;
			if (n->c != NULL)
				result = exec_stmt(n->c);
		}
		return frame_leave(inner, result);
	}
	case AST_EXPRSTMT:
		if (n->a != NULL)
		{
			result = n->a->kind == AST_CALL ? exec_call(n->a) : eval_expr(n->a);
			if (result.kind == FLOW_THROW)
				return result;
			ostack_rewind(result.slot);
		}
		return result_none();
	default:
		die(SYN_ERR);
	}
	return result_none();
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

static void func_free(void *func)
{
	free(func);
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
	ostack = NULL;
	otop = 0;
	ocap = 0;
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
		ostack_rewind(0);
		free(ostack);
		ostack = NULL;
		htab_free(functions, func_free);
		functions = NULL;
		die(RT_OTHER_ERR);
	}
	ostack_rewind(0);
	free(ostack);
	ostack = NULL;
	htab_free(functions, func_free);
	functions = NULL;
}
