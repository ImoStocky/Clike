
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

#include "lexal.h"
#include "types.h"

/**
* Globalni promenna s aktualnim tokenem
*/

token_t scanner;

/**
* Soubor, se kterym pracujem. Nastaven pri initu.
*/
FILE * sourceFile;

/**
* Aktualni pozice v souboru (pri chybe muze ukazat uzivateli radek chyby)
*/
static scannerLocation_s location;
static scannerLocation_s previousLocation;

static char c;
static char prevC;

static uint8_t position = 255;

// buffer pro nazvy promennych a funkci, pripadne nacitani identifikatoru
static string nameBuffer;
static bool nameWasUsed = false;

static string helperString;

//static bool mustBeSpace = false;

// vyskakovatko z whilu
static bool canRun = true;

// jsme treba uprostred stringu a vypadne eof - tak at muzem hlasit chybu
static bool canBeEof = true;


#define SCANNER_KEYWORDS_LENGTH 10
static char * keywords[] = {
		"auto",
		"cin",
		"cout",
		"double",
		"else",
		"for",
		"if",
		"int",
		"return",
		"string"
};

void scanner_init(FILE*);
int scanner_generateToken(token_t*);

static void scanner_tokenReset(void);
static void scanner_bufferPushBack(void);
static void scanner_bufferPushBackPrevious(void);


bool scanner_checkIdentifierChar(char c, bool first) {
	// _, [a-z], [A-Z]
	if(isalpha(c) || c == '_')
	{
		return true;
	}

	// kdyz to neni prvni znak, muze obsahovat i [0-9]
	if( ! first) {
		return (isdigit(c) > 0);
	}

	return false;
}

void scanner_addChar(char c) {
	if(strAddChar(&nameBuffer, c))
	{
		//printf("- %c - ", c);
		exit(INTER_ERR);
	}
}

void scanner_saveStringToToken() {
	scanner.data.lit = nameBuffer.str;
	nameWasUsed = true;
}

int scanner_generateToken(token_t* tok)
{
	// reinicializace tokenu
	scanner_tokenReset();

	// pocatecni stav automatu
	scannerStates_e readingState = S_START;

	// kdyz byl buffer v predchozim kole pouzit, odalokujem a vycistime
	if(nameWasUsed)
	{
		nameWasUsed = false;
		// coz mozna nakonec delat nebudem, protoze by se ztratily data po nacteni tokenu
		// jeste promyslet TODO
		strClear(&nameBuffer);
		//strFree(&nameBuffer);
	}

	// predchozi nacteny znak
	prevC = 0;

	// interni citac pro ruzne srandy
	//unsigned int i = 0;

	while(canRun && ((c = getc(sourceFile)) != EOF))
	{
		if(c == '\n')
		{
			previousLocation.X = location.X;
			previousLocation.Y = location.Y;
			location.Y = 1;
			location.X += 1;
		} else
		{
			previousLocation.Y = location.Y;
			location.Y++;
		}

		switch(readingState)
		{

			case S_START:
				// jednoduche tokeny
				if(c == '+')
				{
					readingState = S_END;
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = PLUS_OP;
				} else if(c == '-')
				{
					readingState = S_END;
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = MINUS_OP;
				} else if(c == '*')
				{
					readingState = S_END;
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = ASTERISK_OP;
				} else if(c == ';')
				{
					readingState = S_END;
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = SEMI_OP;
				} else if(c == ',')
				{
					readingState = S_END;
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = COM_OP;
				} else if(c == '(')
				{
					readingState = S_END;
					scanner.type = BRACE_TK;
					scanner.data.brct = BRACEL_OP;
				} else if(c == ')')
				{
					readingState = S_END;
					scanner.type = BRACE_TK;
					scanner.data.brct = BRACER_OP;
				}  else if(c == '{')
				{
					readingState = S_END;
					scanner.type = BRACE_TK;
					scanner.data.brct = BLOCKL_OP;
				}  else if(c == '}')
				{
					readingState = S_END;
					scanner.type = BRACE_TK;
					scanner.data.brct = BLOCKR_OP;
				}

					// zacatek muze znamenat jiny token
				else if(c == '/')
				{
					readingState = S_SLASH;
				}
				else if(c == '>')
				{
					readingState = S_GREATER;
				} else if(c == '<')
				{
					readingState = S_LOWER;
				} else if(c == '=')
				{
					readingState = S_ASSIGN;
				}

					// specialy a ostatni
				else if(c == '!')
				{
					readingState = S_SCREAMER;
					canBeEof = false;
				} else if(c == '"')
				{
					readingState = S_STRING;
					canBeEof = false;
				}else if(isdigit(c)) {
					/**
					*  NUMBER
					*/
					readingState = S_INTEGER;
					scanner.type = INT_TK;

					scanner_addChar(c);
					scanner_saveStringToToken();

					canBeEof = false;
				} else if(scanner_checkIdentifierChar(c, true)) // kdyz je to platny pocatecni znak identifikatoru
				{
					/**
					*  IDENTIFIER / KEYWORD
					*/
					readingState = S_IDENTIFIER;
					scanner.type = IDENT_TK;

					scanner_addChar(c);
					scanner_saveStringToToken();

					canBeEof = false;
				} else {
					if(isspace(c)) {
						readingState = S_START;
					} else {
						scanner.type = UNKNOWN_TK;
						readingState = S_END;
						canRun = false;
					}
				}

				break;

			case S_STRING:
				if(c == '"') {
					scanner.type = LITERAL_TK;

					scanner_saveStringToToken();

					readingState = S_END;
					canRun = false;
					canBeEof = true;

				} else if(c == '\\') {
					readingState = S_STRING_ESCAPE;
				} else if(c > 31) {
					scanner_addChar(c);
				} else {
					scanner.type = UNKNOWN_TK;
					readingState = S_END;
					canRun = false;
				}
				break;

			case S_STRING_ESCAPE:

				if(c == '"') {
					readingState = S_STRING;
					scanner_addChar(c);
				} else if(c == 'n') {
					readingState = S_STRING;
					scanner_addChar('\n');
				} else if(c == 't') {
					readingState = S_STRING;
					scanner_addChar('\t');
				} else if(c == '\\') {
					readingState = S_STRING;
					scanner_addChar('\\');
				} else if(c == 'x') {
					readingState = S_STRING_ESCAPE_SEQ_START;
				} else {
					scanner.type = UNKNOWN_TK;
					readingState = S_END;
					canRun = false;
				}

				break;

			case S_STRING_ESCAPE_SEQ_START:
				// 0-9a-f
				if(isdigit(c) || (tolower(c) >= 97 && tolower(c) <= 102)) {
					strAddChar(&helperString, tolower(c));
					readingState = S_STRING_ESCAPE_SEQ_END;
				} else {
					scanner.type = UNKNOWN_TK;
					readingState = S_END;
					canRun = false;
				}

				break;

			case S_STRING_ESCAPE_SEQ_END:
				// 0-9a-f
				if(isdigit(c) || (tolower(c) >= 97 && tolower(c) <= 102)) {
					strAddChar(&helperString, tolower(c));
					readingState = S_STRING;

					int number = (int)strtol(strGetStr(&helperString), NULL, 16);

					scanner_addChar((char)number);

					strClear(&helperString);
				} else {
					scanner.type = UNKNOWN_TK;
					readingState = S_END;
					canRun = false;
				}
				break;

			case S_INTEGER:
				if(isdigit(c)) {
					readingState = S_INTEGER;
					// ADD CHAR TO STR
					scanner_addChar(c);
				} else {
					// MAYBE REAL
					if(tolower(c) == 'e') {
						readingState = S_REAL_PLUS_MINUS;
						scanner_addChar(c);
					} else if(c == '.') {
						readingState = S_REAL_DOT_MUST_BE_NUMBER;
						scanner_addChar(c);
					} else {
						if(scanner_checkIdentifierChar(c, true)) {
							exit(LEX_ERR);
						}

						scanner_bufferPushBack();
						scanner_saveStringToToken();

						canRun = false;
						canBeEof = true;
						readingState = S_END;
					}
				}
				break;

			case S_REAL_DOT_MUST_BE_NUMBER:
				if(isdigit(c)) {
					readingState = S_REAL_DOT_NUMBERS;
					scanner_addChar(c);
					scanner.type = REAL_TK;
				} else {
					scanner.type = UNKNOWN_TK;
					readingState = S_END;
					canRun = false;
				}

				break;

			case S_REAL_DOT_NUMBERS:
				if(isdigit(c)) {
					readingState = S_REAL_DOT_NUMBERS;
					scanner_addChar(c);
				} else if(tolower(c) == 'e') {
					readingState = S_REAL_PLUS_MINUS;
					scanner_addChar(c);
				} else {
					scanner.type = REAL_TK;

					scanner_bufferPushBack();
					scanner_saveStringToToken();

					readingState = S_END;
					canRun = false;
				}
				break;

			case S_REAL_PLUS_MINUS:
				if(c == '+' || c == '-' || isdigit(c)) {
					readingState = S_REAL_E_ZERO_NUMBERS;
					scanner_addChar(c);

					if(isdigit(c)) {
						scanner.type = REAL_TK;
						readingState = S_REAL_E_NUMBERS;
					}
				} else {
					scanner_bufferPushBack();
					scanner_bufferPushBackPrevious();

					scanner_saveStringToToken();

					scanner.type = UNKNOWN_TK;
					readingState = S_END;
					canRun = false;
					canBeEof = true;
				}
				break;

			case S_REAL_E_NUMBERS:
				if(isdigit(c))
				{
					readingState = S_REAL_E_NUMBERS;
					scanner_addChar(c);
				} else {
					scanner_bufferPushBack();

					scanner_saveStringToToken();

					readingState = S_END;
					canRun = false;
					canBeEof = true;
				}
				break;

			case S_REAL_E_ZERO_NUMBERS:
				if(isdigit(c)) {
					if(c == '0') {
						readingState = S_REAL_E_ZERO_NUMBERS;
						scanner.type = REAL_TK;
					} else {
						readingState = S_REAL_E_NUMBERS;
						scanner_addChar(c);
						scanner.type = REAL_TK;
					}

				} else {
					scanner.type = UNKNOWN_TK;
					readingState = S_END;
					canRun = false;
				}
				break;

			case S_IDENTIFIER:
				if(scanner_checkIdentifierChar(c, false)) {
					readingState = S_IDENTIFIER;
					scanner_addChar(c);
				} else {
					scanner_bufferPushBack();

					strToLower(&nameBuffer);

					readingState = S_END;
					canRun = false;
					canBeEof = true;

					if((position = strInArray((void **)keywords, SCANNER_KEYWORDS_LENGTH, strGetStr(&nameBuffer))) >= 0) {
						switch(position) {
							case 0 :
								scanner.type = KEYWORD_TK;
								scanner.data.kwd = AUTO_KW;
								break;

							case 1 :
								scanner.type = KEYWORD_TK;
								scanner.data.kwd = CIN_KW;
								break;

							case 2 :
								scanner.type = KEYWORD_TK;
								scanner.data.kwd = COUT_KW;
								break;

							case 3 :
								scanner.type = KEYWORD_TK;
								scanner.data.kwd = DOUBLE_KW;
								break;

							case 4 :
								scanner.type = KEYWORD_TK;
								scanner.data.kwd = ELSE_KW;
								break;
							case 5 :
								scanner.type = KEYWORD_TK;
								scanner.data.kwd = FOR_KW;
								break;
							case 6 :
								scanner.type = KEYWORD_TK;
								scanner.data.kwd = IF_KW;
								break;
							case 7 :
								scanner.type = KEYWORD_TK;
								scanner.data.kwd = INT_KW;
								break;
							case 8 :
								scanner.type = KEYWORD_TK;
								scanner.data.kwd = RETURN_KW;
								break;
							case 9 :
								scanner.type = KEYWORD_TK;
								scanner.data.kwd = STRING_KW;
								break;

							default: break;
						}
					} else
					{
						// not keyword
						scanner_saveStringToToken();
					}
				}
				break;

			case S_SLASH:
				if(c == '/')
				{
					readingState = S_LINE_COMMENT;
					canBeEof = true;
				} else if(c == '*')
				{
					readingState = S_BLOCK_COMMENT;
					canBeEof = false;

				} else {
					scanner.type = OPERATOR_TK;
					scanner.data.kwd = SLASH_OP;

					scanner_bufferPushBack();
					readingState = S_END;
					canRun = false;
					canBeEof = true;
				}
				break;

			case S_LINE_COMMENT:
				if(c == '\n') {
					readingState = S_START;
				} else {
					readingState = S_LINE_COMMENT;
				}

				break;

			case S_BLOCK_COMMENT:
				if(c == '*') {
					readingState = S_BLOCK_COMMENT_STAR;
				} else {
					readingState = S_BLOCK_COMMENT;
				}

				break;

			case S_BLOCK_COMMENT_STAR:
				if(c == '/') {
					readingState = S_START;
					canBeEof = true;
				} else {
					readingState = S_BLOCK_COMMENT;
					scanner_bufferPushBack();
				}

				break;

			case S_SCREAMER:
				if(c == '=')
				{
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = NEQ_OP;
				} else
				{
					scanner.type = UNKNOWN_TK;
				}

				readingState = S_END;
				canRun = false;
				canBeEof = true;
				break;

			case S_ASSIGN:
				if(c == '=')
				{
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = EQ_OP;
				} else
				{
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = ASSIGN_OP;

					scanner_bufferPushBack();
				}

				readingState = S_END;
				canRun = false;
				canBeEof = true;
				break;

			case S_GREATER:
				if(c == '=')
				{
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = GRE_OP;
				} else if(c == '>')
				{
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = DBL_GRE_OP;
				} else {
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = GREAT_OP;
					scanner_bufferPushBack();
				}

				readingState = S_END;
				canRun = false;
				canBeEof = true;
				break;

			case S_LOWER:
				if(c == '<')
				{
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = DBL_LESS_OP;
				} else if(c == '=')
				{
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = LEE_OP;
				} else
				{
					scanner.type = OPERATOR_TK;
					scanner.data.oprtr = LESS_OP;
					scanner_bufferPushBack();
				}

				readingState = S_END;
				canRun = false;
				canBeEof = true;
				break;

			case S_END:
				scanner.type = UNKNOWN_TK;

				canRun = false;

				exit(LEX_ERR);
				break;

		}

		if(readingState == S_END)
		{
			canRun = false;
		}

		prevC = c;
	}

	if(c == EOF)
	{
		strFree(&helperString);
		strFree(&nameBuffer);

		if (canBeEof)
		{
			scanner.type = UNKNOWN_TK;
		} else
		{
			exit(LEX_ERR);
		}
	} else if(scanner.type == UNKNOWN_TK)
	{
		strFree(&helperString);
		strFree(&nameBuffer);
	}

	canBeEof 	= true;
	canRun 		= true;

	position	= 255;

	if(scanner.type == UNKNOWN_TK) {
		return 1;
	}

	memcpy (tok, &scanner, sizeof (token_t));

	if(scanner.type == INT_TK) {
		tok->data.ord = atoi(scanner.data.lit);
	} else if(scanner.type == REAL_TK) {
		tok->data.real = atof(scanner.data.lit);
	} else if(scanner.type == LITERAL_TK) {
		strcpy(tok->data.lit, scanner.data.lit);
	} else if(scanner.type == IDENT_TK) {
		strcpy(tok->data.lit, scanner.data.lit);
	}

	// TODO identifikator patri do symbol table, ne literal
	
	return 0;
}


scannerLocation_s * scanner_getLocation(void)
{
	return &location;
}

void scanner_init(FILE* file)
{
	sourceFile = file;

	strInit(&nameBuffer);
	strInit(&helperString);

	scanner_tokenReset();

	location.X = 1;
	location.Y = 0;

	previousLocation.X = location.X;
	previousLocation.Y = location.Y;
}

static void scanner_tokenReset(void)
{
	scanner.type = UNKNOWN_TK;
}

static void scanner_bufferPushBackPrevious(void)
{
	if(c != ' ')
	{
		ungetc(prevC, sourceFile);

		location.X = previousLocation.X;
		location.Y = previousLocation.Y;
	}
}

static void scanner_bufferPushBack(void)
{
	if(c != ' ')
	{
		ungetc(c, sourceFile);

		location.X = previousLocation.X;
		location.Y = previousLocation.Y;
	}
}
