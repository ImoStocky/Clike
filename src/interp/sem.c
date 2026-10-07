#include "interp_priv.h"

#include <string.h>

static void register_func(interp_t *ip, ast_t *fn)
{
	func_t *old = htab_get(ip->functions, fn->name);
	if (old != NULL)
	{
		if (old->native)
		{
			compiler_fail(ip->comp, SEM_ERR);
			return;
		}
		if (old->def != NULL && old->def->b != NULL && fn->b != NULL)
		{
			compiler_fail(ip->comp, SEM_ERR);
			return;
		}
		if (fn->b != NULL)
			old->def = fn;
		return;
	}
	{
		func_t *f = xmalloc(sizeof(*f));
		memset(f, 0, sizeof(*f));
		f->def = fn;
		htab_put(ip->functions, fn->name, f);
	}
}

void sem_prepare(interp_t *ip, ast_t *prog)
{
	ast_t *fn;
	func_t *mainfn;

	for (fn = prog->a; fn != NULL && ip->comp->status == COMP_OK; fn = fn->next)
		register_func(ip, fn);

	mainfn = htab_get(ip->functions, "main");
	if (ip->comp->status == COMP_OK &&
		(mainfn == NULL || mainfn->def == NULL || mainfn->def->b == NULL))
		compiler_fail(ip->comp, SEM_ERR);
	if (ip->comp->status == COMP_OK && mainfn->def->a != NULL)
		compiler_fail(ip->comp, SEM_TYPE_ERR);
}
