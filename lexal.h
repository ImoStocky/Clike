#ifndef IFJ_LEXAL_H
#define IFJ_LEXAL_H

#include <stdio.h>
#include "str.h"
#include "types.h"

typedef struct scanner_s
{
	FILE *file;
	string buf;
	int have_unread;
	int unread_c;
	int active;
} scanner_t;

errv_t scanner_init(scanner_t *scanner, FILE *file);
void scanner_destroy(scanner_t *scanner);
errv_t scanner_generateToken(scanner_t *scanner, token_t *tok);

#endif
