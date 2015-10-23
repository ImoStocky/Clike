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
	SYN_ERR,					//!< Invalid program stucture error
	SEM_ERR,					//!< Semantic error, undefined or redefinition of function/variable 
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
	CONTROL_TK,
	BRACE_TK,
	UNKNOWN_TK
}token_kind_t;

/*! \enum keyword_e
 *	\brief Ordinal values for keywords 
 *  On keyword detection scanner shall return token of kind KEYWORD_TK with 
 *  specific value from these
 *  No need to allocate string for type of keyword that never changes
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
	ASTERISK_OP = 0,		//!< "*"
	SLASH_OP,				//!< "/"
	PLUS_OP, 
	MINUS_OP,
	LESS_OP,				//!< "<"
	GREAT_OP,				//!< ">"
	LEE_OP,					//!< "<="
	GRE_OP,					//!< ">="
	EQ_OP,					//!< "=="
	NEQ_OP,					//!< "!="
	ASSIGN_OP				//!< "="
}operator_t;

/*! \enum brace_e
 *	\brief Ordinal values for braces
 *	On brace detection scanner shall return BRACE_TK 
 */
typedef enum brace_e
{
	BLOCKL_OP,					//!< "{"
	BLOCKR_OP,					//!< "}"
	BRACEL_OP,					//!< "("
	BRACER_OP,					//!< ")"
	SQ_BRACEL_OP,				//!< "["
	SQ_BRACER_OP				//!< "]"
}brace_t;

/*! \struct token_s
 *	\brief Structure to represent token returned by scaner
 *	Token contains a morfing variable. Data are interpreted according to token
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
		keywd_t kwd;
		operator_t oprtr;
		brace_t brct;
		xtable_t* symbol;
	}data;
}token_t;


#define Galloc(size, ptr) ( (ptr)=malloc(size) != NULL ? 0 : exit(99), 1)




#endif //IFJ_TYPES_H

