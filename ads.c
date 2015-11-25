/*! \file ads.c
 *  \brief Advanced data structures module to make life easier
 *	
 *	This module is supposed to hold all necesay complicated data types 
 *	for compiler machinery to work flawlessly
 */

#include "ads.h"
#include <stdbool.h>
#include <string.h>

static uint32_t sdbm(unsigned char* str)
{
	uint32_t hash = 0;
	int c;
	while (c = *str++)
		hash = c + (hash << 6) + (hash << 16) - hash;
	return hash;
}

// Add stack with default type of int, 
// Chceck implementation

int xs_init(xstack_t** st, size_t size)
{
	*st = malloc(sizeof(xs_t));
	if(*st != NULL){
		void* tmp = malloc(al4B(size)*sizeof(base_t));
		if(tmp != NULL){
			(*st)->size = al4B(size); //!< number of dwords
			(*st)->top = 0;
			(*st)->data = tmp;
			return st;
		}
		free(*st);
		free(tmp);
	}
	return 1;
}

void xs_del(xstack_t** st)
{
	free((*st)->data);
	free((*st);
	*st = NULL;
}
			

int xs_resize(xstack_t* st, int size)
{
	unsigned ne = al4B(size);
	if(st->top < ne){
		void* tmp = realloc(st->data, ne*sizeof(st->data[0]));
		if(tmp != NULL){
			st->data = tmp;
			st->size = ne;
			return 0;
		}
	}
	return 1;
}

int xs_push(xstack_t* st, void* data, int length)
{
	unsigned ne = al4B(length);
	if(st->top >= ne && st->top + ne <= st->size){
		memcpy(st->data+st->top, data, length);
		st->top += ne; 
		return 0;
	} else {
		while(ne + st->top > st->size){
			int error = xs_resize(st, st->size*2);
			if(error){
				fprintf(stderr, "Failed to allocate %u memory for xs_t\n",st->size*2);
				return 1;
			}
		}
		memcpy(st->data+st->top, data, length);
		st->top += ne; 
		return 0;
	}
	return 1;
}

int xs_pop(xstack_t* st, int length)
{
	unsigned ne = al4B(length);
	if(st->top >= ne && st->top + ne <= st->size){
		st->top -= ne;
		return 0;
	}
	return 1;
}

int xs_top(xstack_t* st, void* data, int length)
{	
	unsigned ne = al4B(length);
	if(st->top >= ne && st->top + ne <= st->size){
		memcpy(data, st->data+st->top-ne, length);
		return 0;
	}
	return 1;
}

int xs_tpop(xstack_t* st, void* data, int length){

	unsigned ne = al4B(length);
	if(st->top >= ne && st->top + ne <= st->size){
		memcpy(data, st->data+st->top-ne, length);
		st->top -= ne;
		return 0;
	}
	return 1;
}

int xs_empty(xstack_t* st)
{	
	return(st->top == 0);
}


int si_init(istack_t** st, size_t size)
{
	*st = malloc(sizeof(istack_t));
	if(*st != NULL){
		void* tmp = malloc(size*sizeof(int));
		if(tmp != NULL){
			(*st)->size = size; //!< number of dwords
			(*st)->top = 0;
			(*st)->data = tmp;
			return 0;
		}
		free(*st);
		free(tmp);
	}
	return 1;
}

void si_del(istack_t** st)
{
	free((*st)->data);
	free((*st);
	*st = NULL;
}
			

int si_resize(istack_t* st, int size)
{
	if(st->top < size){
		void* tmp = realloc(st->data, size*sizeof(st->data[0]));
		if(tmp != NULL){
			st->data = tmp;
			st->size = size;
			return 0;
		}
	}
	return 1;
}

void si_push(istack_t* st, int data )
{
	if(st->top + 1 < st->size){
		*(st->data + st->top) = data;
		st->top++; 
	}
}

void si_pop(istack_t* st)
{
	if(st->top > 0) 
		st->top-- ;
}

void si_top(istack_t* st, int* data)
{	
	if(st->top > 0){
		*data = *(st->data + st->top-1);
	}
}

void si_tpop(istack_t* st, int* data){

	if(st->top > 0){
		st->top-- ;
		*data = *(st->data + st->top);
	}
}

int si_empty(istack_t* st)
{	
	return(st->top == 0);
}

int si_full(istack* st)
{
	return(st->top < st->size);
}
