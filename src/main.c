#include "compiler.h"
#include "interp.h"
#include "syntal.h"

#include <stdio.h>

int main(int argc, char **argv)
{
	compiler_t compiler;
	errv_t status;

	compiler_init(&compiler);
	if (argc > 2)
		return INTER_ERR;
	if (argc == 2)
	{
		compiler.in = fopen(argv[1], "r");
		if (compiler.in == NULL)
			return INTER_ERR;
		compiler.own_in = 1;
	}
	else
		compiler.in = stdin;

	status = scanner_init(&compiler.scan, compiler.in);
	if (status == COMP_OK)
		status = parse_program(&compiler);
	if (status == COMP_OK)
		status = interpret(&compiler);
	compiler_cleanup(&compiler);
	return (int)status;
}
