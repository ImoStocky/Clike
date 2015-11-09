/*! \file ads.c
 *  \brief Advanced data structures module to make life easier
 *	
 *	This module is supposed to hold all necesay complicated data types 
 *	for compiler machinery to work flawlessly
 */

#include "ads.h"
#include <stdbool.h>

//! \define Rounds bytelen to smallest possible multiple of bytesize of boundary type
#define aligned(bytelen, boundary)\
	(bytelen/sizeof(boundary)+(length%sizeof(boundary) > 0 ? 1 : 0))

static uint32_t sdbm(unsigned char* str)
{
	uint32_t hash = 0;
	int c;

	while (c = *str++)
		hash = c + (hash << 6) + (hash << 16) - hash;

	return hash;
}

int xstack_init(stack_t** st, size_t size)
{
	if(st != NULL && *st != NULL){
		*st = malloc(sizeof(stack_t));
		void* tmp = malloc(aligned(size, uint32_t)*sizeof(uint32_t));
		if(*st != NULL && tmp != NULL){
			(*st)->size = aligned(size, uint32_t); //!< number of dwords
			(*st)->top = 0;
			(*st)->data = tmp;
			return 0;
		}
		free(*st);
		free(tmp);
	}
	return 1;
}

int xstack_resize(stack_t* st, int size)
{
	int ne = aligned(size, uint32_t);
	if(st->top <= ne){
		void* tmp = realloc(st->data, ne*sizeof(uint32_t));
		if(tmp != NULL){
			st->data = tmp;
			st->size = ne;
			return 0;
		}
	}
	return 1;
}

int xstack_push(xstack_t* st, void* data, int length)
{
	int ne = aligned(length, uint32_t);
	if(st->top >= ne && st->top + ne <= st->size){
		memcpy(st->data+st->top, data, length);
		st->top += ne; 
		return 0;
	}
	return 1;
}

int xstack_pop(xstack_t* st, int length)
{
	int ne = aligned(length, uint32_t);
	if(st->top >= ne && st->top + ne <= st->size){
		st->top -= ne;
		return 0;
	}
	return 1;
}

int xstack_top(xstack_t* st, void* data, int length)
{	
	if(st->top >= ne && st->top + ne <= st->size){
		memcpy(data, st->data+st->top-ne, length);
		return 0;
	}
	return 1;
}

bool xstack_empty(xstack_t* st)
{	
	return(st->top == 0);
}

xtable_t *xtable_init(size_t size)
{
	
}

int xtable_insert(xtable_t* tab, int key, void* data)
{
	
}

bool xtable_search(xtable_t* tab, int key)
{
	
}

int xtable_delete(xtable_t* tab, int key)
{

}

int xtable_copy(xtable_t* tab, int key, void* data)
{

}

int xtable_resize(xtable_t* tab, size_t num)
{

}
