
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

#include "global.h"
#include "scanner.h"

/**
* Globalni promenna s aktualnim tokenem
*/
tokenInfo_s scannerActualToken;

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

void scanner_init(void);
tokenInfo_s * scanner_generateToken(void);
scannerLocation_s * scanner_getLocation(void);

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
		global_error_print("Realloc / strAddChar error in LEX.");
		global_exit(EXIT_INTERNAL_ERROR);
	}
}

void scanner_saveStringToToken() {
	scannerActualToken.length = (unsigned int)strGetLength(&nameBuffer);
	scannerActualToken.data = &nameBuffer;
	nameWasUsed = true;
}

tokenInfo_s * scanner_generateToken(void)
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

		/*if(mustBeSpace) {
			if(isspace(c)) {
				mustBeSpace = false;
			} else {
				global_error_print("LEX: Char had to be space.\n");
				global_exit(EXIT_LEX_ERROR);
			}
		}*/

		switch(readingState)
		{

			case S_START:
				scannerActualToken.rowStart = location.X;
				scannerActualToken.colStart = location.Y;

				// jednoduche tokeny
				if(c == '+')
				{
					readingState = S_END;
					scannerActualToken.type = T_OPERATOR_ADD;
				} else if(c == '-')
				{
					readingState = S_END;
					scannerActualToken.type = T_OPERATOR_SUB;
				} else if(c == '*')
				{
					readingState = S_END;
					scannerActualToken.type = T_OPERATOR_MUL;
				} else if(c == ';')
				{
					readingState = S_END;
					scannerActualToken.type = T_SEMICOLON;
				} else if(c == ',')
				{
					readingState = S_END;
					scannerActualToken.type = T_COMMA;
				} else if(c == '(')
				{
					readingState = S_END;
					scannerActualToken.type = T_LEFT_BRACKET;
				} else if(c == ')')
				{
					readingState = S_END;
					scannerActualToken.type = T_RIGHT_BRACKET;
				}  else if(c == '{')
				{
					readingState = S_END;
					scannerActualToken.type = T_LEFT_CURLY_BRACKET;
				}  else if(c == '}')
				{
					readingState = S_END;
					scannerActualToken.type = T_RIGHT_CURLY_BRACKET;
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
					scannerActualToken.type = T_VALUE_INTEGER;

					scanner_addChar(c);
					scanner_saveStringToToken();

					canBeEof = false;
				} else if(scanner_checkIdentifierChar(c, true)) // kdyz je to platny pocatecni znak identifikatoru
				{
					/**
					*  IDENTIFIER / KEYWORD
					*/
					readingState = S_IDENTIFIER;
					scannerActualToken.type = T_IDENTIFIER;

					scanner_addChar(c);
					scanner_saveStringToToken();

					canBeEof = false;
				} else {
					if(isspace(c)) {
						readingState = S_START;
					} else {
						scannerActualToken.type = T_ERROR;
						readingState = S_END;
						canRun = false;
					}
				}

				break;

			case S_STRING:
				if(c == '"') {
					scannerActualToken.type = T_VALUE_STRING;

					scanner_saveStringToToken();

					readingState = S_END;
					canRun = false;
					canBeEof = true;

				} else if(c == '\\') {
					readingState = S_STRING_ESCAPE;
				} else if(c > 31) {
					scanner_addChar(c);
				} else {
					scannerActualToken.type = T_ERROR;
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
					scannerActualToken.type = T_ERROR;
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
					scannerActualToken.type = T_ERROR;
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
					scannerActualToken.type = T_ERROR;
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
							global_error_print("LEX: Char had to be space.\n");
							global_exit(EXIT_LEX_ERROR);
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
					scannerActualToken.type = T_VALUE_REAL;
				} else {
					scannerActualToken.type = T_ERROR;
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
					scannerActualToken.type = T_VALUE_REAL;

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
						scannerActualToken.type = T_VALUE_REAL;
						readingState = S_REAL_E_NUMBERS;
					}
				} else {
					scanner_bufferPushBack();
					scanner_bufferPushBackPrevious();

					scanner_saveStringToToken();

					scannerActualToken.type = T_ERROR;
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
						scannerActualToken.type = T_VALUE_REAL;
					} else {
						readingState = S_REAL_E_NUMBERS;
						scanner_addChar(c);
						scannerActualToken.type = T_VALUE_REAL;
					}

				} else {
					scannerActualToken.type = T_ERROR;
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
							case 0 : scannerActualToken.type = T_KEYWORD_AUTO; break;
							case 1 : scannerActualToken.type = T_KEYWORD_CIN; break;
							case 2 : scannerActualToken.type = T_KEYWORD_COUT; break;
							case 3 : scannerActualToken.type = T_KEYWORD_DOUBLE; break;
							case 4 : scannerActualToken.type = T_KEYWORD_ELSE;  break;
							case 5 : scannerActualToken.type = T_KEYWORD_FOR; break;
							case 6 : scannerActualToken.type = T_KEYWORD_IF; break;
							case 7 : scannerActualToken.type = T_KEYWORD_INT; break;
							case 8 : scannerActualToken.type = T_KEYWORD_RETURN; break;
							case 9 : scannerActualToken.type = T_KEYWORD_STRING; break;

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
					scannerActualToken.type = T_OPERATOR_DIV;
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
					scannerActualToken.type = T_NOT_EQUAL;
				} else
				{
					scannerActualToken.type = T_ERROR;
				}

				readingState = S_END;
				canRun = false;
				canBeEof = true;
				break;

			case S_ASSIGN:
				if(c == '=')
				{
					scannerActualToken.type = T_EQUAL;
				} else
				{
					scannerActualToken.type = T_ASSIGN;
					scanner_bufferPushBack();
				}

				readingState = S_END;
				canRun = false;
				canBeEof = true;
				break;

			case S_GREATER:
				if(c == '=')
				{
					scannerActualToken.type = T_GREATER_OR_EQUAL;
				} else if(c == '>')
				{
					scannerActualToken.type = T_ARROW_RIGHT;
				} else {
					scannerActualToken.type = T_GREATER;
					scanner_bufferPushBack();
				}

				readingState = S_END;
				canRun = false;
				canBeEof = true;
				break;

			case S_LOWER:
				if(c == '<')
				{
					scannerActualToken.type = T_ARROW_LEFT;
				} else if(c == '=')
				{
					scannerActualToken.type = T_LOWER_OR_EQUAL;
				} else
				{
					scannerActualToken.type = T_LOWER;
					scanner_bufferPushBack();
				}

				readingState = S_END;
				canRun = false;
				canBeEof = true;
				break;

			case S_END:
				scannerActualToken.type = T_ERROR;

				canRun = false;

				global_error_print("Went to S_END in LEX.");
				global_exit(EXIT_LEX_ERROR);
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
			scannerActualToken.type = T_EOF;
		} else
		{
			global_error_print("Cannot be EOF here (LEX).");
			global_exit(EXIT_LEX_ERROR);
		}
	} else if(scannerActualToken.type == T_ERROR)
	{
		strFree(&helperString);
		strFree(&nameBuffer);
	}

	canBeEof 	= true;
	canRun 		= true;

	position	= 255;

	return &scannerActualToken;
}


int scanner_isError(void)
{
	return (scannerActualToken.type == T_EOF || scannerActualToken.type == T_ERROR);
}

scannerLocation_s * scanner_getLocation(void)
{
	return &location;
}

void scanner_init(void)
{
	sourceFile = global_getSourceFileHandler();

	strInit(&nameBuffer);
	strInit(&helperString);

	scanner_tokenReset();

	location.X = 1;
	location.Y = 0;

	previousLocation.X = location.X;
	previousLocation.Y = location.Y;
}

void scanner_debug(void) {
	scanner_generateToken();
	tokenInfo_s t = scannerActualToken;

	while(t.type != T_EOF && t.type != T_ERROR) {

		if(t.data != NULL && strGetStr(t.data) != NULL)
			printf("%s\n", strGetStr(t.data));

		t = *(scanner_generateToken());
		printf("t: %i\n", t.type);
	}
}

tokenInfo_s* scanner_copyToken(tokenInfo_s copied)
{
	tokenInfo_s* newToken = (tokenInfo_s*)malloc(sizeof(tokenInfo_s));

	newToken->type = copied.type;

	/*if(copied.data != NULL) {
		CREATE_STRING(tes);
		strArrToString(&tes, strGetStr(copied.data));

		printf("- %s -\n", strGetStr(copied.data));
		cn ++;

		if(cn == 4)
		exit(0);
		//newToken->data = &tes;
		//strCopyString(newToken->data, copied.data);
	}*/

	newToken->length = copied.length;
	newToken->rowStart = copied.rowStart;
	newToken->colStart = copied.colStart;

	return newToken;
}

static void scanner_tokenReset(void)
{
	scannerActualToken.type = T_ERROR;
	scannerActualToken.data = NULL;
	scannerActualToken.length = 0;
	scannerActualToken.rowStart = location.X;
	scannerActualToken.colStart = location.Y;
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
