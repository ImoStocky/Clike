#ifndef _ADS_H
#define _ADS_H

#include <stdlib.h>
#include <string.h>


typedef struct xstack_s
{
	unsigned size;
	unsigned top;
	uint32_t data[1];
}xstack_t;

int xstack_init(stack_t** arr, size_t size, size_t elem);
int xstack_resize(stack_t** arr, int n_size);
int xstack_push(xstack_t* arr, void* data, int length);
int xstack_pop(xstack_t* arr, int length);
int xstack_top(xstack_t* arr, void* data, int length);
bool xstack_empty(xstack_t* arr);


typedef struct xtable_s
{
	unsigned size;
	unsigned population;
	unsigned elem;
	uint32_t data[1];
}xtable_s;

int xtable_init(xtable_t** arr, size_t num, size_t elem);
int xtable_insert(xtable_t* arr, int key, void* data);
int xtable_delete(xtable_t* arr, int key);
int xtable_copy(xtable_t* arr, int key, void* data);
int xtable_resize(xtable_t** arr, size_t num, size_t elem);
bool xtable_search(xtable_t* arr, int key);

#define SETb(arr, i, v) ( arr[i/bsizeof(arr[0])] = \
		arr[i/bsizeof(arr[0])] & ~((uint64_t)1 << i%bsizeof(arr[0])) | ((-v) & (uint64_t)1 << i%bsizeof(arr[0])) ) 
#define GETb(arr, i) ( arr[i/bsizeof(arr[0])] = arr[i/bsizeof(arr[0])] >> i%bsizeof(arr[0]) & 1 )

#endif // _ADS_H
