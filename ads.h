#ifndef _ADS_H
#define _ADS_H

#include <stdlib.h>
#include <string.h>

/*! \struct xstack_s
 *	\brief Stack ADS, aligned to 4B
 *	Structure describing stack data type
 *	Should behave according to ial stack signature
 */
typedef struct xstack_s
{
	unsigned size;	//!< Size of stack in number of elements
	unsigned top;	//!< Top of stack as offset of data member
	uint32_t* data;	//!< Base of stack
xstack_t;

/*! \brief Initialize stack
 *	\param st Stack data structure
 *	\param size Size of stack to alocate
 */
int xstack_init(stack_t* st, size_t size);
int xstack_resize(stack_t* st, int size);

/*! \biref Push data to stack
 *	\param st Stack data structure
 *	\int length Length of data in bytes
 */
int xstack_push(xstack_t* st, void* data, int length);

/*! \biref Remove data from stack
 *	\param st Stack data structure
 *	\param data General pointer
 *	\int length Length of data in bytes
 */
int xstack_pop(xstack_t* st, int length);

/*! \biref Retrieve data from to of the stack
 *	\param st Stack data structure
 *	\param data Location where to store retrieved data
 *	\int length Length of data in bytes
 */
int xstack_top(xstack_t* st, void* data, int length);

/*! \biref True if stack is empty
 *	\param st Stack data structure
 */
bool xstack_empty(xstack_t* st);


typedef struct xtable_s
{
	unsigned size;
	unsigned population;
	unsigned elem;
	uint32_t* data;
}xtable_s;

int xtable_init(xtable_t* tab, size_t num);
int xtable_insert(xtable_t* tab, int key, void* data);
int xtable_delete(xtable_t* tab, int key);
int xtable_copy(xtable_t* tab, int key, void* data);
int xtable_resize(xtable_t* tab, size_t num, size_t elem);
bool xtable_search(xtable_t* tab, int key);

#define SETb(arr, i, v) ( arr[i/bsizeof(arr[0])] = \
		arr[i/bsizeof(arr[0])] & ~((uint64_t)1 << i%bsizeof(arr[0])) | ((-v) & (uint64_t)1 << i%bsizeof(arr[0])) ) 
#define GETb(arr, i) ( arr[i/bsizeof(arr[0])] = arr[i/bsizeof(arr[0])] >> i%bsizeof(arr[0]) & 1 )

#endif // _ADS_H
