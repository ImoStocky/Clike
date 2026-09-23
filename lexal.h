#ifndef IFJ_LEXAL_H
#define IFJ_LEXAL_H

#include <stdio.h>
#include "types.h"

void scanner_init(FILE *file);
int scanner_generateToken(token_t *tok);

#endif
