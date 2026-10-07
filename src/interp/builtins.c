#include "interp_priv.h"

#include <string.h>

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

static value_t v_string(char *s)
{
	value_t v = v_undef();
	v.type = TY_STRING;
	v.init = 1;
	v.s = s;
	return v;
}

static int nat_length(interp_t *ip, value_t *args, int n, value_t *out)
{
	(void)ip;
	if (n != 1 || args[0].type != TY_STRING || !args[0].init)
		return 1;
	*out = v_int((int)strlen(args[0].s ? args[0].s : ""));
	return 0;
}

static int nat_concat(interp_t *ip, value_t *args, int n, value_t *out)
{
	size_t len;
	char *s;
	if (n != 2 || args[0].type != TY_STRING || args[1].type != TY_STRING)
		return 1;
	if (!args[0].init || !args[1].init)
	{
		compiler_fail(ip->comp, RT_NO_INIT_ERR);
		return 1;
	}
	len = strlen(args[0].s ? args[0].s : "") + strlen(args[1].s ? args[1].s : "");
	s = xmalloc(len + 1);
	strcpy(s, args[0].s ? args[0].s : "");
	strcat(s, args[1].s ? args[1].s : "");
	*out = v_string(s);
	return 0;
}

static int nat_substr(interp_t *ip, value_t *args, int n, value_t *out)
{
	int i, m, len;
	char *s;
	const char *src;
	if (n != 3 || args[0].type != TY_STRING || args[1].type != TY_INT || args[2].type != TY_INT)
		return 1;
	if (!args[0].init || !args[1].init || !args[2].init)
	{
		compiler_fail(ip->comp, RT_NO_INIT_ERR);
		return 1;
	}
	src = args[0].s ? args[0].s : "";
	len = (int)strlen(src);
	i = args[1].i;
	m = args[2].i;
	if (i < 0 || m < 0)
	{
		compiler_fail(ip->comp, RT_OTHER_ERR);
		return 1;
	}
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

static int nat_find(interp_t *ip, value_t *args, int n, value_t *out)
{
	if (n != 2 || args[0].type != TY_STRING || args[1].type != TY_STRING)
		return 1;
	if (!args[0].init || !args[1].init)
	{
		compiler_fail(ip->comp, RT_NO_INIT_ERR);
		return 1;
	}
	*out = v_int(kmp_find(args[0].s ? args[0].s : "", args[1].s ? args[1].s : ""));
	return 0;
}

static int nat_sort(interp_t *ip, value_t *args, int n, value_t *out)
{
	char *s;
	(void)ip;
	if (n != 1 || args[0].type != TY_STRING || !args[0].init)
		return 1;
	s = xstrdup(args[0].s ? args[0].s : "");
	heap_sort_chars(s);
	*out = v_string(s);
	return 0;
}

static void add_native(interp_t *ip, const char *name, int (*fn)(interp_t *, value_t *, int, value_t *))
{
	func_t *f = xmalloc(sizeof(*f));
	memset(f, 0, sizeof(*f));
	f->native = 1;
	f->nat = fn;
	htab_put(ip->functions, name, f);
}

void builtins_register(interp_t *ip)
{
	add_native(ip, "length", nat_length);
	add_native(ip, "concat", nat_concat);
	add_native(ip, "substr", nat_substr);
	add_native(ip, "find", nat_find);
	add_native(ip, "sort", nat_sort);
}
