#include "ast.h"
#include "interp.h"
#include "lexal.h"
#include "syntal.h"
#include "types.h"

#include <stdio.h>

int main(int argc, char **argv)
{
	FILE *in = stdin;
	ast_t *prog;

	if (argc > 2)
		return INTER_ERR;
	if (argc == 2)
	{
		in = fopen(argv[1], "r");
		if (in == NULL)
			return INTER_ERR;
	}

	scanner_init(in);
	prog = parse_program();
	interpret(prog);
	ast_free(prog);
	scanner_destroy();
	if (in != stdin)
		fclose(in);
	return COMP_OK;
}
