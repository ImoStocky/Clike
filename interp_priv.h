#ifndef IFJ_INTERP_PRIV_H
#define IFJ_INTERP_PRIV_H

#include "compiler.h"
#include "ial.h"

typedef struct value_s
{
	dtype_t type;
	int init;
	int i;
	double d;
	char *s;
} value_t;

typedef struct interp_s interp_t;

typedef struct func_s
{
	int native;
	int (*nat)(interp_t *ip, value_t *args, int n, value_t *out);
	ast_t *def;
} func_t;

typedef struct frame_s
{
	htab_t *vars;
	struct frame_s *parent;
	size_t base;
} frame_t;

struct interp_s
{
	compiler_t *comp;
	htab_t *functions;
	frame_t *cur_frame;
	value_t *ostack;
	size_t otop;
	size_t ocap;
};

void builtins_register(interp_t *ip);
void sem_prepare(interp_t *ip, ast_t *prog);

#endif
