
/*! \file types.h
 *	\brief Compiler essential types and enums
 *	More coment here l8tr
 *	Credits to xstoff02 bitches !
 */

#ifndef IFJ_TYPES_H
#define IFJ_TYPES_H

#include <stdlib.h>

/*! \enum errv_e
 *  \brief Return codes for compiler
 *
 */
typedef enum errv_e
{
	COMP_OK = 0,
	LEX_ERR = 1,
	SYN_ERR,
	SEM_ERR,
	SEM_TYPE_ERR,
	AMB_TYPE_ERR,
	SEM_OTHER_ERR,
	RT_NUM_ERR,
	RT_NO_INIT_ERR,
	RT_ZDIV_ERR,
	RT_OTHER_ERR,
	INTER_ERR = 99
}errv_t;

/*! \enum tokend_kind_e 
 *	\brief Basic types of tokens
 * 	Scanner shall return tokens that belongs to one of these types
 */
typedef enum token_kind_e
{
	KEYWORD_TK = 0,
	INT_TK,
	REAL_TK,
	LITERAL_TK,
	OPERATOR_TK,
	IDENT_TK,
	SEMICOLON_TK,
	UNKNOWN_TK
}token_kind_t;

/*! \enum keyword_e
 *	\brief Ordinal values for keywords 
 *  On keyword detection scanner shall return token of kind KEYWORD_TK with 
 *  specific value from these
 */
typedef enum keyword_e
{
	AUTO_KW = 0,
	CIN_KW,
	COUT_KW,
	INT_KW,
	DOUBLE_KW,
	STRING_KW,
	IF_KW,
	ELSE_KW,
	FOR_KW,
	RETURN_KW
}keywd_t;

/*! \enum operator_e 
 *	\brief Ordinal values for operators
 *	On operator detection scanner shall return token of kind OPERATOR_TK with
 *	specific value from these
 */
typedef enum operator_e
{
	MUL_OP = 0,
	DIV_OP,
	PLUS_OP,
	MINUS_OP,
	LESS_OP,
	GREAT_OP,
	LEE_OP,
	GRE_OP,
	EQ_OP,
	NEQ_OP 
}operator_t;

/*! \struct token_s
 *	\brief Structure to represent token returned by scaner
 *
 */
typedef struct token_s
{
	token_kind_t type;
	union
	{
		int ord;
		double real;
		char* lit;
		keywd_t kwd;
		operator_t oprtr;
	}data;
}token_t;


#define Galloc(size, ptr) ( (ptr)=malloc(size) != NULL ? 0 : exit(99), 1)





#endif //IFJ_TYPES_H

