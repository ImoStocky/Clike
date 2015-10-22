
#ifndef SCANNER_H_
#define SCANNER_H_

#include "string.h"

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

// typy tokenu
typedef enum
{
    /* 0  */ T_ERROR,            // standardni vraceny, pokud to neprepise neco jineho
    /* 1  */ T_EOF,              // posledni token

    /* 2  */ T_IDENTIFIER,       // identifikator

    // klicove slova
    /* 3  */ T_KEYWORD_AUTO,
    /* 4  */ T_KEYWORD_CIN,
    /* 5  */ T_KEYWORD_COUT,
    /* 6  */ T_KEYWORD_DOUBLE,
    /* 7  */ T_KEYWORD_ELSE,
    /* 8  */ T_KEYWORD_FOR,
    /* 9  */ T_KEYWORD_IF,
    /* 10 */ T_KEYWORD_INT,
    /* 11 */ T_KEYWORD_RETURN,
    /* 12 */ T_KEYWORD_STRING,

    /* 13 */ T_OPERATOR_ADD,     // scitani
    /* 14 */ T_OPERATOR_SUB,     // odcitani (nebo taky unarni minus? uvidime)
    /* 15 */ T_OPERATOR_MUL,     // nasobeni
    /* 16 */ T_OPERATOR_DIV,     // deleni

    /* 17 */ T_SEMICOLON,        // strednik
    /* 18 */ T_COMMA,            // carka

    /* 19 */ T_LEFT_BRACKET,     // leva kulata (
    /* 20 */ T_RIGHT_BRACKET,    // prava kulata )

    /* 21 */ T_LEFT_CURLY_BRACKET,     // leva slozena {
    /* 22 */ T_RIGHT_CURLY_BRACKET,    // prava slozena }

    /* 23 */ T_ASSIGN,           // prirazeni =

    /* 24 */ T_ARROW_LEFT,       // <<
    /* 25 */ T_ARROW_RIGHT,      // >>

    /* 26 */ T_EQUAL,            // porovnani ==
    /* 27 */ T_NOT_EQUAL,        // nerovna se !=
    /* 28 */ T_GREATER,          // vetsitko >
    /* 29 */ T_GREATER_OR_EQUAL, // vetsi nebo rovno >=
    /* 30 */ T_LOWER,            // mensitko <
    /* 31 */ T_LOWER_OR_EQUAL,   // mensi nebo rovni <=

    /* 32 */ T_VALUE_INTEGER,    // hodnota typu integer
    /* 33 */ T_VALUE_REAL,       // hodnota typu real
    /* 34 */ T_VALUE_STRING,     // hodnota typu string

} tokenState_e;

// informace k jednotlivemu tokenu
typedef struct
{
    tokenState_e type;      // typ tokenu
    string * data;            // data tokenu
    unsigned int length;    // delka
    unsigned int rowStart;  // pocatecni umisteni - radek   \ pro vypis
    unsigned int colStart;  // pocatecni umisteni - sloupec / chyby?
} tokenInfo_s;

// aktualni poloha v souboru (pouzit, kdyz nastane chyba, at to muze vratit kde treba je :-)
typedef struct
{
    unsigned int X;
    unsigned int Y;
} scannerLocation_s;

// globalni promenna
extern tokenInfo_s scannerActualToken;

// inicializace
void scanner_init(void);

void scanner_debug(void);

tokenInfo_s* scanner_copyToken(tokenInfo_s);

// je vracen nejaky z chybovych stavu
int scanner_isError(void);

// nacti dalsi token
tokenInfo_s * scanner_generateToken(void);

// vrat strukturu s mistem ukazatele v souboru
scannerLocation_s * scanner_getLocation(void);

#endif /* SCANNER_H_ */
