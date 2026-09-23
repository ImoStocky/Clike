#ifndef IFJ_LEXAL_H
#define IFJ_LEXAL_H

#include <stdio.h>
#include "types.h"

void scanner_init(FILE *file);
void scanner_destroy(void);
int scanner_generateToken(token_t *tok);

#endif
