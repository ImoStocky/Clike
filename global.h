
#ifndef GLOBAL_H_
#define GLOBAL_H_

#define EXIT_SUCCESS 0
#define EXIT_LEX_ERROR 1
#define EXIT_SYNTAX_ERROR 2
#define EXIT_SEM_UNDEFINED_OR_REDEFINED 3
#define EXIT_SEM_DATA_TYPE_MISMATCH 4
#define EXIT_SEM_OTHER_ERROR 5
#define EXIT_RUNTIME_READ_ERROR 6
#define EXIT_RUNTIME_UNINITIALIZED_ERROR 7
#define EXIT_RUNTIME_ZERO_DIVISION 8
#define EXIT_RUNTIME_OTHER_ERROR 9
#define EXIT_INTERNAL_ERROR 99

#define MSG_SYN_ERROR "Syntax ERROR.\n"

extern FILE * globalFileHandler;

void global_free(void);
void global_exit(int exitCode);
void global_error_print(char * text);

void global_setSourceFileHandler(FILE * sourceFile);
FILE *global_getSourceFileHandler(void);


#endif /* GLOBAL_H_ */
