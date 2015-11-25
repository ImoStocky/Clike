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
 *  Error codes as requred by task definition
 */
typedef enum errv_e
{
	COMP_OK = 0,
	LEX_ERR = 1,
	SYN_ERR,			//!< Invalid program stucture error
	SEM_ERR,			//!< Semantic error, undefined or redefinition of function/variable
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
 *	Scanner shall return tokens that belongs to one of these types
 */
typedef enum token_kind_e
{
	//EOF-LIKE TOKEN
	END_TK = 0,	

	//PRECEDENCE GRAMMAR TAGS
	EQ_PREC,
	LT_PREC,
	GR_PREC,
	NO_PREC,
	//VALID TERMINALS
	AUTO_KW,
	CIN_KW,
	COUT_KW,
	INT_KW,
	DOUBLE_KW,
	STRING_KW,
	IF_KW,
	ELSE_KW,
	FOR_KW,
	RETURN_KW,

	IDENT_TK,
	INT_TK,
	REAL_TK,
	LITERAL_TK,

	ASTERISK_OP,		//!< "*"
	SLASH_OP,			//!< "/"
	PLUS_OP,
	MINUS_OP,
	SEMI_OP,			//! ";" semicolon
	COM_OP,				//! "," comma
	DBL_LESS_OP,		//! "<<"
	DBL_GRE_OP,			//! ">>"
	LESS_OP,			//!< "<"
	GREAT_OP,			//!< ">"
	LEE_OP,				//!< "<="
	GRE_OP,				//!< ">="
	EQ_OP,				//!< "=="
	NEQ_OP,				//!< "!="
	ASSIGN_OP,			//!< "="
	
	BRACEL_OP,			//!< "("
	BRACER_OP,			//!< ")"

	BLOCKL_OP,			//!< "{"
	BLOCKR_OP,			//!< "}"
	EXPR,

	//GRAMMAR NONTERMINALS FOLLOW
	NTS,
	PROG,
	PROG_N,
	FUNC,
	FD_PARAM,
	FD_PARAM_N,
	FUNC_REST,
	STAT_BLOCK,
	STAT_N,
	STAT,
	COUT_EXPR_N,
	ID_LIST,
	IF_BODY,
	ELSE_OR_NOT,
	ELSE_BODY,
	VAR_DEF,
	TYPE,	
	NTE		//rule end;

}token_kind_t;

/*! \struct token_s
 *	\brief Structure to represent token returned by scaner
 *	Token contains a morfing variable. Data member is interpreted according to token
 *	type
 */

typedef struct token_s
{
	token_kind_t type;
	union
	{
		int ord;
		double real;
		char* lit;
		void* attr;
	}data;
}token_t;



//! \brief No chceck memory allocation exit on failure
#define Galloc(ptr, size) ( (ptr)=malloc(size) != NULL ? 0 : exit(99), 1)

#endif //IFJ_TYPES_H

