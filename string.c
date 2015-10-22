//jednoducha knihovna pro praci s nekonecne dlouhymi retezci
#include <string.h>
#include <malloc.h>
#include <stdarg.h>
#include <ctype.h>

#include "string.h"

#define STR_LEN_INC 32
// konstanta STR_LEN_INC udava, na kolik bytu provedeme pocatecni alokaci pameti
// pokud nacitame retezec znak po znaku, pamet se postupne bude alkokovat na
// nasobky tohoto cisla 

#define STR_ERROR   1
#define STR_SUCCESS 0

int strInit(string *s)
// funkce vytvori novy retezec
{
   if ((s->str = (char*) malloc(STR_LEN_INC)) == NULL)
      return STR_ERROR;
      
   s->str[0] = '\0';
   s->length = 0;
   s->allocSize = STR_LEN_INC;
   
   return STR_SUCCESS;
}

char *strdup (const char *s) {
    char *d = malloc (strlen (s) + 1);
    
    if (d == NULL) 
		return NULL;
    strcpy (d,s);
    return d;
}

char *storePrintf (const char *fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);
    size_t sz = snprintf(NULL, 0, fmt, arg);
    char *buf = (char *)malloc(sz + 1);
    if(buf != NULL)
    {
		vsprintf(buf, fmt, arg);
		va_end (arg);	
		return buf;    	
	} else
	{
		return NULL;
	}


}

void strFree(string *s)
// funkce uvolni retezec z pameti
{
   free(s->str);
}

void strClear(string *s)
// funkce vymaze obsah retezce
{
   s->str[0] = '\0';
   s->length = 0;
}

int strAddChar(string *s1, char c)
// prida na konec retezce jeden znak
{
   if (s1->allocSize - 1 <= s1->length)
   {
      // pamet nestaci, je potreba provest realokaci
      if ((s1->str = (char*) realloc(s1->str, s1->length + STR_LEN_INC)) == NULL)
         return STR_ERROR;
      s1->allocSize = s1->length + STR_LEN_INC;
   }
   s1->str[s1->length] = c;
   s1->length++;
   s1->str[s1->length] = '\0';

   return STR_SUCCESS;
}

int strAddStr(string *dest, string *c)
// prida na konec retezce jeden znak
{
    int size= c->length;
    for (int i = 0; i < size; i++)
    {
        strAddChar(dest, c->str[i]);
    }

    return STR_SUCCESS;
}

int strArrToString(string *s1, char *c)
// prida na konec retezce jeden znak
{
  strClear(s1);
  size_t size=strlen(c);
  for (int i = 0; i < size; i++)
  {
    if (s1->allocSize <= s1->length + 1)
    {
      // pamet nestaci, je potreba provest realokaci
      if ((s1->str = (char*) realloc(s1->str, s1->length + STR_LEN_INC)) == NULL)
        return STR_ERROR;
      s1->allocSize = s1->length + STR_LEN_INC;
    }
    strAddChar(s1, c[i]);
  }
    s1->length++;
    s1->str[s1->length] = '\0';
   return STR_SUCCESS;
}

int strCopyString(string *s1, string *s2)
// prekopiruje retezec s2 do s1
{
   int newLength = s2->length;
   if (newLength >= s1->allocSize)
   {
      // pamet nestaci, je potreba provest realokaci
      if ((s1->str = (char*) realloc(s1->str, newLength + 1)) == NULL)
         return STR_ERROR;
      s1->allocSize = newLength + 1;
   }
   strcpy(s1->str, s2->str);
   s1->length = newLength;
   return STR_SUCCESS;
}

string tempString;

string * strCopy(string * copied)
{
    strInit(&tempString);

    strCopyString(&tempString, copied);

    return &tempString;
}


int strCmpStringChar(string *s1, char *s2)
// porovna oba retezce a vrati vysledek
{
    return strcmp(s1->str, s2);
}

int strCmpString(string *s1, string *s2)
// porovna oba retezce a vrati vysledek
{
   return strcmp(s1->str, s2->str);
}

int strCmpConstStr(string *s1, char* s2)
// porovna nas retezec s konstantnim retezcem
{
   return strcmp(s1->str, s2);
}

char *strGetStr(string *s)
// vrati textovou cast retezce
{
   return s->str;
}

int strGetLength(string *s)
// vrati delku daneho retezce
{
   return s->length;
}

void strToLower(string *s)
{
	for(int i = 0; s->str[i]; i++){
		s->str[i] = tolower(s->str[i]);
	}
}

int strInArray(void *array[], int size, void *lookfor)
{
	int i;

	for (i = 0; i < size; i++)
		if (strcmp(lookfor, array[i]) == 0)
			return i;

	return -1;
}
