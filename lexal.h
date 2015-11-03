#ifndef IFJ_LEXAL_H
#define IFJ_LEXAL_H

#include "types.h"
#include "str.h"

// jednotlive stavy automatu
typedef enum
{
	S_START,            // standardni start

	S_SLASH,
	S_LINE_COMMENT,
	S_BLOCK_COMMENT,
	S_BLOCK_COMMENT_STAR,

	S_ASSIGN,
	S_GREATER,
	S_LOWER,

	S_SCREAMER,

	S_IDENTIFIER,

	S_INTEGER,

	S_REAL_PLUS_MINUS,
	S_REAL_DOT_MUST_BE_NUMBER,
	S_REAL_DOT_NUMBERS,
	S_REAL_E_ZERO_NUMBERS,
	S_REAL_E_NUMBERS,

	S_STRING,
	S_STRING_ESCAPE,
	S_STRING_ESCAPE_SEQ_START,
	S_STRING_ESCAPE_SEQ_END,

	S_END,              // pomocny ukoncovaci

} scannerStates_e;

// aktualni poloha v souboru (pouzit, kdyz nastane chyba, at to muze vratit kde treba je :-)
typedef struct
{
	unsigned int X;
	unsigned int Y;
} scannerLocation_s;

// inicializace
void scanner_init(FILE*);

// nacti dalsi token
token_t scanner_generateToken(void);


token_t scanner;

#endif //IFJ_LEXAL_H
