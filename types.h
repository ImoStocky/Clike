#ifndef IFJ_TYPES_H
#define IFJ_TYPES_H

#include <stdlib.h>

typedef enum errv_e
{
	COMP_OK = 0,
	LEX_ERR = 1,
	SYN_ERR = 2,
	SEM_ERR = 3,
	SEM_TYPE_ERR = 4,
	AMB_TYPE_ERR = 5,
	SEM_OTHER_ERR = 6,
	RT_NUM_ERR = 7,
	RT_NO_INIT_ERR = 8,
	RT_ZDIV_ERR = 9,
	RT_OTHER_ERR = 10,
	INTER_ERR = 99
} errv_t;

typedef enum dtype_e
{
	TY_VOID = 0,
	TY_INT,
	TY_DOUBLE,
	TY_STRING,
	TY_AUTO
} dtype_t;

typedef enum token_kind_e
{
	END_TK = 0,

	AUTO_KW,
	CIN_KW,
	COUT_KW,
	INT_KW,
	DOUBLE_KW,
	STRING_KW,
	IF_KW,
	ELSE_KW,
	FOR_KW,
	WHILE_KW,
	DO_KW,
	RETURN_KW,

	IDENT_TK,
	INT_TK,
	REAL_TK,
	LITERAL_TK,

	ASTERISK_OP,
	SLASH_OP,
	PERCENT_OP,
	PLUS_OP,
	MINUS_OP,
	SEMI_OP,
	COM_OP,
	DBL_LESS_OP,
	DBL_GRE_OP,
	LESS_OP,
	GREAT_OP,
	LEE_OP,
	GRE_OP,
	EQ_OP,
	NEQ_OP,
	ASSIGN_OP,
	AND_OP,
	OR_OP,
	NOT_OP,

	BRACEL_OP,
	BRACER_OP,
	BLOCKL_OP,
	BLOCKR_OP
} token_kind_t;

typedef struct token_s
{
	token_kind_t type;
	union
	{
		int ord;
		double real;
		char *lit;
	} data;
} token_t;

void *xmalloc(size_t n);
char *xstrdup(const char *s);
void die(errv_t e);

#endif
