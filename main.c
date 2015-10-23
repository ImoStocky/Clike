/**
* project: ifj15
* members: xvymet03, logins, ...
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "scanner.h"

int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		global_error_print("Wrong parameters, show --help for information.\n");
		return EXIT_INTERNAL_ERROR;
	}
	
	if(strlen(argv[1]) == 6 && strcmp(argv[1], "--help") == 0)
	{
		printf("napoveda? jestli nejaka bude\n");
		return EXIT_INTERNAL_ERROR;
	}

	FILE *file = fopen(argv[1], "r");
	if (file == NULL)
	{
		global_error_print("File not found!\n");
		return EXIT_INTERNAL_ERROR;
	}

	// predame handler 
	global_setSourceFileHandler(file);
	
	// lex. analyzator - inicializace
	scanner_init();
	
	tokenInfo_s * token = scanner_generateToken();

	while(token->type != T_EOF && token->type != T_ERROR) {
		printf("Token OK - %u\n", token->type);
		
		token = scanner_generateToken();
	}

	//printf("%d \n", tokenType);
	if(token->type == T_ERROR){
		global_error_print("Token ERROR in LEX.");
		global_exit(1);
	}

	return EXIT_SUCCESS;
}
