
//hlavickovy soubor pro praci s nekonecne dlouhymi retezci

#ifndef STRING_H
#define STRING_H

typedef struct
{
  char* str;		// misto pro dany retezec ukonceny znakem '\0'
  int length;		// skutecna delka retezce
  int allocSize;	// velikost alokovane pameti
} string;


int strInit(string *s);
void strFree(string *s);

void strClear(string *s);
int strAddChar(string *s1, char c);
int strCopyString(string *s1, string *s2);
int strCmpStringChar(string *s1, char *s2);
int strCmpString(string *s1, string *s2);
int strCmpConstStr(string *s1, char *s2);
int strArrToString(string *s1, char *c);

int strAddStr(string *dest, string *c);

char *strGetStr(string *s);
int strGetLength(string *s);

char *storePrintf (const char *fmt, ...);
char *strdup (const char *s);

typedef int (*cmpfunc)(void *, void *);

string *strCopy(string *);

void strToLower(string *s);
int strInArray(void *array[], int size, void *lookfor);

#endif

