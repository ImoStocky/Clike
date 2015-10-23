
#include <stdio.h>
#include <stdlib.h>

#include "global.h"


FILE * globalFileHandler = NULL;

void global_setSourceFileHandler(FILE * sourceFile)
{
	globalFileHandler = sourceFile;
}

FILE * global_getSourceFileHandler(void)
{
	return globalFileHandler;
}

/**
 * Odalokovani
 */
void global_free(void)
{
	// zavrem soubor
	fclose(global_getSourceFileHandler());
}

/**
 * Kdyz nastane nejaka chyba, zavolat toto, to se postara o odalokovani pameti a navraceni chyboveho kodu.
 */
void global_exit(int exitCode)
{
	global_free();
	exit(exitCode); // ukonci s predanym status codem
}

/**
 * At se s tim porad nemusime psat
 */
void global_error_print(char * text)
{
	fprintf(stderr, text);
}
