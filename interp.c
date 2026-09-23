#include "interp.h"
#include "interp_priv.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static size_t ostack_push(interp_t *ip, value_t value)
{
	if (ip->otop == ip->ocap)
	{
		size_t ncap = ip->ocap == 0 ? 32 : ip->ocap * 2;
		value_t *grown = realloc(ip->ostack, ncap * sizeof(*ip->ostack));
		if (grown == NULL)
			{ compiler_fail(ip->comp, INTER_ERR); return 0; }
		ip->ostack = grown;
		ip->ocap = ncap;
	}
	ip->ostack[ip->otop] = value;
	return ip->otop++;
}

static value_t ostack_take(interp_t *ip, size_t slot)
{
	value_t value = ip->ostack[slot];
	ip->ostack[slot] = v_undef();
	return value;
}

static void ostack_rewind(interp_t *ip, size_t marker)
{
	while (ip->otop > marker)
	{
		ip->otop--;
		value_destroy(&ip->ostack[ip->otop]);
	}
}

static exec_result_t result_value(interp_t *ip, flow_kind_t kind, value_t value)
{
	exec_result_t result;
	result.kind = kind;
	result.slot = ostack_push(ip, value);
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

static frame_t *frame_push(interp_t *ip, frame_t *parent, size_t base)
{
	frame_t *f = xmalloc(sizeof(*f));
	memset(f, 0, sizeof(*f));
	f->vars = htab_init(32);
	f->parent = parent;
	f->base = base;
	ip->cur_frame = f;
	return f;
}

static void frame_pop(interp_t *ip, frame_t *f)
{
	ip->cur_frame = f->parent;
	htab_free(f->vars, NULL);
	free(f);
}

static exec_result_t frame_leave(interp_t *ip, frame_t *frame, exec_result_t result)
{
	if (result.kind != FLOW_NORMAL)
	{
		value_t saved = ostack_take(ip, result.slot);
		ostack_rewind(ip, frame->base);
		result.slot = ostack_push(ip, saved);
	}
	else
		ostack_rewind(ip, frame->base);
	frame_pop(ip, frame);
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

static void bind_slot(interp_t *ip, frame_t *frame, const char *name, size_t slot)
{
	if (htab_get(frame->vars, name) != NULL ||
		htab_put(frame->vars, name, slot_ptr(slot)))
		{ compiler_fail(ip->comp, SEM_ERR); return; }
}

static int truthy(interp_t *ip, value_t v)
{
	if (!v.init)
		{ compiler_fail(ip->comp, RT_NO_INIT_ERR); return 1; }
	if (v.type == TY_INT)
		return v.i != 0;
	if (v.type == TY_DOUBLE)
		return v.d != 0.0;
	if (v.type == TY_STRING)
		return v.s != NULL && v.s[0] != '\0';
	{ compiler_fail(ip->comp, SEM_TYPE_ERR); return 1; }
	return 0;
}

static exec_result_t eval_expr(interp_t *ip, ast_t *n);
static exec_result_t exec_stmt(interp_t *ip, ast_t *n);

static void assign_into(interp_t *ip, value_t *dst, value_t src)
{
	if (!src.init)
		{ compiler_fail(ip->comp, RT_NO_INIT_ERR); return; }
	if (dst->type == TY_AUTO)
		dst->type = src.type;
	if (dst->type == TY_INT)
	{
		if (src.type == TY_INT)
			dst->i = src.i;
		else if (src.type == TY_DOUBLE)
			dst->i = (int)src.d;
		else
			{ compiler_fail(ip->comp, SEM_TYPE_ERR); return; }
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
			{ compiler_fail(ip->comp, SEM_TYPE_ERR); return; }
		free(dst->s);
		dst->s = NULL;
	}
	else if (dst->type == TY_STRING)
	{
		if (src.type != TY_STRING)
			{ compiler_fail(ip->comp, SEM_TYPE_ERR); return; }
		free(dst->s);
		dst->s = xstrdup(src.s ? src.s : "");
	}
	else
		{ compiler_fail(ip->comp, SEM_TYPE_ERR); return; }
	dst->init = 1;
}

static value_t num_bin(interp_t *ip, token_kind_t op, value_t a, value_t b)
{
	int use_d;
	double da, db;
	int ia, ib;
	if (!a.init || !b.init)
		{ compiler_fail(ip->comp, RT_NO_INIT_ERR); return v_undef(); }
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
		{ compiler_fail(ip->comp, SEM_TYPE_ERR); return v_undef(); }
	}
	if ((a.type != TY_INT && a.type != TY_DOUBLE) ||
		(b.type != TY_INT && b.type != TY_DOUBLE))
		{ compiler_fail(ip->comp, SEM_TYPE_ERR); return v_undef(); }

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
	if (op == AND_OP) return v_int(truthy(ip, a) && truthy(ip, b));
	if (op == OR_OP) return v_int(truthy(ip, a) || truthy(ip, b));

	if (op == SLASH_OP)
	{
		if (use_d)
		{
			if (db == 0.0)
				{ compiler_fail(ip->comp, RT_ZDIV_ERR); return v_undef(); }
			return v_double(da / db);
		}
		if (ib == 0)
			{ compiler_fail(ip->comp, RT_ZDIV_ERR); return v_undef(); }
		return v_int(ia / ib);
	}
	if (op == PERCENT_OP)
	{
		if (use_d)
			{ compiler_fail(ip->comp, SEM_TYPE_ERR); return v_undef(); }
		if (ib == 0)
			{ compiler_fail(ip->comp, RT_ZDIV_ERR); return v_undef(); }
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
	{ compiler_fail(ip->comp, SEM_TYPE_ERR); return v_undef(); }
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

static exec_result_t exec_call(interp_t *ip, ast_t *call)
{
	func_t *fn;
	ast_t *param;
	ast_t *arg;
	frame_t *caller;
	frame_t *frame;
	exec_result_t result;
	size_t arg_base = ip->otop;
	int n = 0;
	int i;

	if (call->name == NULL)
		{ compiler_fail(ip->comp, SEM_ERR); return result_none(); }
	fn = htab_get(ip->functions, call->name);
	if (fn == NULL)
		{ compiler_fail(ip->comp, SEM_ERR); return result_none(); }

	for (arg = call->a; arg != NULL; arg = arg->next)
	{
		result = eval_expr(ip, arg);
		if (result.kind == FLOW_THROW)
		{
			value_t thrown = ostack_take(ip, result.slot);
			ostack_rewind(ip, arg_base);
			return result_value(ip, FLOW_THROW, thrown);
		}
		n++;
		if (n > 32)
			{ compiler_fail(ip->comp, SEM_OTHER_ERR); return result_none(); }
	}

	if (fn->native)
	{
		value_t out = v_undef();
		int error = fn->nat(ip, ip->ostack + arg_base, n, &out);
		ostack_rewind(ip, arg_base);
		if (error != 0)
			{ compiler_fail(ip->comp, SEM_TYPE_ERR); return result_none(); }
		return result_value(ip, FLOW_NORMAL, out);
	}
	if (fn->def == NULL || fn->def->b == NULL)
		{ compiler_fail(ip->comp, SEM_ERR); return result_none(); }
	if (count_list(fn->def->a) != n)
		{ compiler_fail(ip->comp, SEM_TYPE_ERR); return result_none(); }

	param = fn->def->a;
	for (i = 0; i < n; i++)
	{
		value_t slot = v_undef();
		slot.type = param->dtype;
		assign_into(ip, &slot, ip->ostack[arg_base + (size_t)i]);
		value_destroy(&ip->ostack[arg_base + (size_t)i]);
		ip->ostack[arg_base + (size_t)i] = slot;
		param = param->next;
	}

	caller = ip->cur_frame;
	frame = frame_push(ip, NULL, arg_base);
	param = fn->def->a;
	for (i = 0; i < n; i++)
	{
		bind_slot(ip, frame, param->name, arg_base + (size_t)i);
		param = param->next;
	}
	result = frame_leave(ip, frame, exec_stmt(ip, fn->def->b));
	ip->cur_frame = caller;
	if (result.kind == FLOW_RETURN)
		result.kind = FLOW_NORMAL;
	else if (result.kind == FLOW_NORMAL)
		result = result_value(ip, FLOW_NORMAL, v_undef());
	return result;
}

static exec_result_t eval_expr(interp_t *ip, ast_t *n)
{
	size_t marker;
	exec_result_t left;
	exec_result_t right;
	value_t out;
	int condition;

	if (n == NULL)
		{ compiler_fail(ip->comp, SYN_ERR); return result_none(); }
	switch (n->kind)
	{
	case AST_INT:
		return result_value(ip, FLOW_NORMAL, v_int(n->ival));
	case AST_DOUBLE:
		return result_value(ip, FLOW_NORMAL, v_double(n->dval));
	case AST_STRING:
		return result_value(ip, FLOW_NORMAL, v_string(xstrdup(n->sval ? n->sval : "")));
	case AST_IDENT:
	{
		size_t slot;
		value_t copy;
		if (!lookup_slot(ip->cur_frame, n->name, &slot))
			{ compiler_fail(ip->comp, SEM_ERR); return result_none(); }
		if (!ip->ostack[slot].init)
			{ compiler_fail(ip->comp, RT_NO_INIT_ERR); return result_none(); }
		copy = ip->ostack[slot];
		if (copy.type == TY_STRING)
			copy.s = xstrdup(copy.s ? copy.s : "");
		return result_value(ip, FLOW_NORMAL, copy);
	}
	case AST_UNOP:
		marker = ip->otop;
		left = eval_expr(ip, n->a);
		if (left.kind == FLOW_THROW)
			return left;
		if (n->op == NOT_OP)
			out = v_int(!truthy(ip, ip->ostack[left.slot]));
		else if (n->op == MINUS_OP)
		{
			if (!ip->ostack[left.slot].init)
				{ compiler_fail(ip->comp, RT_NO_INIT_ERR); return result_none(); }
			if (ip->ostack[left.slot].type == TY_INT)
				out = v_int(-ip->ostack[left.slot].i);
			else if (ip->ostack[left.slot].type == TY_DOUBLE)
				out = v_double(-ip->ostack[left.slot].d);
			else
				{ compiler_fail(ip->comp, SEM_TYPE_ERR); return result_none(); }
		}
		else
		{
			{ compiler_fail(ip->comp, SYN_ERR); return result_none(); }
			out = v_undef();
		}
		ostack_rewind(ip, marker);
		return result_value(ip, FLOW_NORMAL, out);
	case AST_BINOP:
		marker = ip->otop;
		left = eval_expr(ip, n->a);
		if (left.kind == FLOW_THROW)
			return left;
		if (n->op == AND_OP || n->op == OR_OP)
		{
			condition = truthy(ip, ip->ostack[left.slot]);
			ostack_rewind(ip, marker);
			if ((n->op == AND_OP && !condition) || (n->op == OR_OP && condition))
				return result_value(ip, FLOW_NORMAL, v_int(n->op == OR_OP));
			right = eval_expr(ip, n->b);
			if (right.kind == FLOW_THROW)
				return right;
			condition = truthy(ip, ip->ostack[right.slot]);
			ostack_rewind(ip, right.slot);
			return result_value(ip, FLOW_NORMAL, v_int(condition));
		}
		right = eval_expr(ip, n->b);
		if (right.kind == FLOW_THROW)
		{
			value_t thrown = ostack_take(ip, right.slot);
			ostack_rewind(ip, marker);
			return result_value(ip, FLOW_THROW, thrown);
		}
		out = num_bin(ip, n->op, ip->ostack[left.slot], ip->ostack[right.slot]);
		ostack_rewind(ip, marker);
		return result_value(ip, FLOW_NORMAL, out);
	case AST_CALL:
		left = exec_call(ip, n);
		if (left.kind == FLOW_THROW)
			return left;
		if (!ip->ostack[left.slot].init)
			{ compiler_fail(ip->comp, RT_NO_INIT_ERR); return result_none(); }
		return left;
	default:
		{ compiler_fail(ip->comp, SYN_ERR); return result_none(); }
	}
	return result_none();
}

static exec_result_t exec_vardecl(interp_t *ip, ast_t *n)
{
	value_t slot;
	size_t index;
	exec_result_t init;

	if (htab_get(ip->cur_frame->vars, n->name) != NULL)
		{ compiler_fail(ip->comp, SEM_ERR); return result_none(); }
	memset(&slot, 0, sizeof(slot));
	slot.type = n->dtype;
	index = ostack_push(ip, slot);
	bind_slot(ip, ip->cur_frame, n->name, index);
	if (n->a != NULL)
	{
		init = eval_expr(ip, n->a);
		if (init.kind == FLOW_THROW)
			return init;
		assign_into(ip, &ip->ostack[index], ip->ostack[init.slot]);
		ostack_rewind(ip, init.slot);
	}
	else if (n->dtype == TY_AUTO)
		{ compiler_fail(ip->comp, AMB_TYPE_ERR); return result_none(); }
	return result_none();
}

static exec_result_t exec_assign(interp_t *ip, ast_t *n)
{
	size_t dst;
	exec_result_t value;

	if (!lookup_slot(ip->cur_frame, n->name, &dst))
		{ compiler_fail(ip->comp, SEM_ERR); return result_none(); }
	value = eval_expr(ip, n->a);
	if (value.kind == FLOW_THROW)
		return value;
	assign_into(ip, &ip->ostack[dst], ip->ostack[value.slot]);
	ostack_rewind(ip, value.slot);
	return result_none();
}

static exec_result_t exec_cin(interp_t *ip, ast_t *n)
{
	ast_t *id;
	for (id = n->a; id != NULL; id = id->next)
	{
		size_t dst;
		if (!lookup_slot(ip->cur_frame, id->name, &dst))
			{ compiler_fail(ip->comp, SEM_ERR); return result_none(); }
		if (ip->ostack[dst].type == TY_AUTO)
			{ compiler_fail(ip->comp, AMB_TYPE_ERR); return result_none(); }
		if (ip->ostack[dst].type == TY_INT)
		{
			int x;
			if (scanf("%d", &x) != 1)
				{ compiler_fail(ip->comp, RT_NUM_ERR); return result_none(); }
			ip->ostack[dst].i = x;
			ip->ostack[dst].init = 1;
		}
		else if (ip->ostack[dst].type == TY_DOUBLE)
		{
			double x;
			if (scanf("%lf", &x) != 1)
				{ compiler_fail(ip->comp, RT_NUM_ERR); return result_none(); }
			ip->ostack[dst].d = x;
			ip->ostack[dst].init = 1;
		}
		else if (ip->ostack[dst].type == TY_STRING)
		{
			char tmp[4096];
			if (scanf("%4095s", tmp) != 1)
				{ compiler_fail(ip->comp, RT_NUM_ERR); return result_none(); }
			free(ip->ostack[dst].s);
			ip->ostack[dst].s = xstrdup(tmp);
			ip->ostack[dst].init = 1;
		}
		else
			{ compiler_fail(ip->comp, SEM_TYPE_ERR); return result_none(); }
	}
	return result_none();
}

static exec_result_t exec_cout(interp_t *ip, ast_t *n)
{
	ast_t *e;
	for (e = n->a; e != NULL; e = e->next)
	{
		exec_result_t result = eval_expr(ip, e);
		value_t *value;
		if (result.kind == FLOW_THROW)
			return result;
		value = &ip->ostack[result.slot];
		if (!value->init)
			{ compiler_fail(ip->comp, RT_NO_INIT_ERR); return result_none(); }
		if (value->type == TY_INT)
			printf("%d", value->i);
		else if (value->type == TY_DOUBLE)
			printf("%g", value->d);
		else if (value->type == TY_STRING)
			fputs(value->s ? value->s : "", stdout);
		else
			{ compiler_fail(ip->comp, SEM_TYPE_ERR); return result_none(); }
		ostack_rewind(ip, result.slot);
	}
	return result_none();
}

static exec_result_t exec_stmt(interp_t *ip, ast_t *n)
{
	exec_result_t result;
	if (n == NULL)
		return result_none();
	switch (n->kind)
	{
	case AST_BLOCK:
	{
		frame_t *inner = frame_push(ip, ip->cur_frame, ip->otop);
		ast_t *statement;
		result = result_none();
		for (statement = n->a; statement != NULL && result.kind == FLOW_NORMAL; statement = statement->next)
			result = exec_stmt(ip, statement);
		return frame_leave(ip, inner, result);
	}
	case AST_VARDECL:
		return exec_vardecl(ip, n);
	case AST_ASSIGN:
		return exec_assign(ip, n);
	case AST_CIN:
		return exec_cin(ip, n);
	case AST_COUT:
		return exec_cout(ip, n);
	case AST_RETURN:
		if (n->a == NULL)
			return result_value(ip, FLOW_RETURN, v_undef());
		result = eval_expr(ip, n->a);
		if (result.kind == FLOW_NORMAL)
			result.kind = FLOW_RETURN;
		return result;
	case AST_THROW:
		result = eval_expr(ip, ast_throw_expr(n));
		if (result.kind == FLOW_THROW)
			return result;
		if (!ip->ostack[result.slot].init)
			{ compiler_fail(ip->comp, RT_NO_INIT_ERR); return result_none(); }
		if (ip->ostack[result.slot].type != TY_STRING)
		{
			ostack_rewind(ip, result.slot);
			{ compiler_fail(ip->comp, SEM_TYPE_ERR); return result_none(); }
		}
		result.kind = FLOW_THROW;
		return result;
	case AST_TRY:
		result = exec_stmt(ip, ast_try_body(n));
		if (result.kind == FLOW_THROW)
		{
			value_t thrown = ostack_take(ip, result.slot);
			frame_t *catch_frame;
			size_t base;
			ostack_rewind(ip, result.slot);
			base = ip->otop;
			catch_frame = frame_push(ip, ip->cur_frame, base);
			bind_slot(ip, catch_frame, n->name, ostack_push(ip, thrown));
			result = frame_leave(ip, catch_frame, exec_stmt(ip, ast_try_handler(n)));
		}
		return result;
	case AST_IF:
	{
		int take_then;
		result = eval_expr(ip, n->a);
		if (result.kind == FLOW_THROW)
			return result;
		take_then = truthy(ip, ip->ostack[result.slot]);
		ostack_rewind(ip, result.slot);
		return exec_stmt(ip, take_then ? n->b : n->c);
	}
	case AST_WHILE:
		for (;;)
		{
			int keep_going;
			result = eval_expr(ip, n->a);
			if (result.kind == FLOW_THROW)
				return result;
			keep_going = truthy(ip, ip->ostack[result.slot]);
			ostack_rewind(ip, result.slot);
			if (!keep_going)
				return result_none();
			result = exec_stmt(ip, n->b);
			if (result.kind != FLOW_NORMAL)
				return result;
		}
	case AST_DO:
		for (;;)
		{
			int keep_going;
			result = exec_stmt(ip, n->a);
			if (result.kind != FLOW_NORMAL)
				return result;
			result = eval_expr(ip, n->b);
			if (result.kind == FLOW_THROW)
				return result;
			keep_going = truthy(ip, ip->ostack[result.slot]);
			ostack_rewind(ip, result.slot);
			if (!keep_going)
				return result_none();
		}
	case AST_FOR:
	{
		frame_t *inner = frame_push(ip, ip->cur_frame, ip->otop);
		result = result_none();
		if (n->a != NULL)
			result = exec_stmt(ip, n->a);
		while (result.kind == FLOW_NORMAL)
		{
			if (n->b != NULL)
			{
				exec_result_t condition = eval_expr(ip, n->b);
				int keep_going;
				if (condition.kind == FLOW_THROW)
				{
					result = condition;
					break;
				}
				keep_going = truthy(ip, ip->ostack[condition.slot]);
				ostack_rewind(ip, condition.slot);
				if (!keep_going)
					break;
			}
			result = exec_stmt(ip, n->d);
			if (result.kind != FLOW_NORMAL)
				break;
			if (n->c != NULL)
				result = exec_stmt(ip, n->c);
		}
		return frame_leave(ip, inner, result);
	}
	case AST_EXPRSTMT:
		if (n->a != NULL)
		{
			result = n->a->kind == AST_CALL ? exec_call(ip, n->a) : eval_expr(ip, n->a);
			if (result.kind == FLOW_THROW)
				return result;
			ostack_rewind(ip, result.slot);
		}
		return result_none();
	default:
		{ compiler_fail(ip->comp, SYN_ERR); return result_none(); }
	}
	return result_none();
}
static void func_free(void *func)
{
	free(func);
}

static void interp_release(interp_t *ip)
{
	while (ip->cur_frame != NULL)
	{
		frame_t *frame = ip->cur_frame;
		ip->cur_frame = frame->parent;
		htab_free(frame->vars, NULL);
		free(frame);
	}
	if (ip->ostack != NULL)
	{
		ostack_rewind(ip, 0);
		free(ip->ostack);
		ip->ostack = NULL;
	}
	if (ip->functions != NULL)
	{
		htab_free(ip->functions, func_free);
		ip->functions = NULL;
	}
}

errv_t interpret(compiler_t *c)
{
	interp_t ip;
	ast_t call;
	exec_result_t result;

	memset(&ip, 0, sizeof(ip));
	ip.comp = c;
	ip.functions = htab_init(64);
	builtins_register(&ip);
	sem_prepare(&ip, c->prog);

	if (c->status == COMP_OK)
	{
		memset(&call, 0, sizeof(call));
		call.kind = AST_CALL;
		call.name = "main";
		result = exec_call(&ip, &call);
		if (c->status == COMP_OK && result.kind == FLOW_THROW)
			compiler_fail(c, RT_OTHER_ERR);
	}
	interp_release(&ip);
	return c->status;
}
