/*! \file ads.h
 *  \brief Advanced data structures module to make life easier
 *	
 *	This module is supposed to hold all necesay complicated data types 
 *	for compiler machinery to work flawlessly
 */

#include <stdint.h>

#define al4B(bytelen)\
	(bytelen/sizeof(base_t)+(bytelen%sizeof(base_t) > 0 ? 1 : 0))


#define ELNb(d, i) (i/8*sizeof(d[0]))
#define bIDX(d, i) (i%8*sizeof(d[0]))
#define GETb(d, i) (d[ELNb(d,i)] & 1LL << bIDX(d,i))
#define SETb(d, i, m) (d[ELNb(d,i)] = ((d[ELNb(d,i)] & ~(m << bIDX(d,i))) | (-d[ELNb(d,i)] & (m << bIDX(d,i)))) )

typedef enum symbol_type_e
{
	FUNC_TP = 1,
	VAR_TP	
}symbol_type_t;

typedef struct symbol_s{
		char* ident;
		int s_type;		
		int d_type;
		union{
			void* var_addr; 
			stack_t func_params;
		}
}symbol_t;

typedef uint32_t base_t;

typedef struct xstack_s{
	unsigned size;
	unsigned top;
	base_t *data;
}xstack_t;

typedef struct istack_s{
	unsigned size;
	unsigned top;
	int *data;
}istack_t;



int sx_init(xstack_t** st, size_t size);
void sx_del(xstack_t** st);
int sx_resize(xstack_t* st, int size);
int sx_push(xstack_t* st, void* data, int length);
int sx_pop(xstack_t* st, int length);
int sx_top(xstack_t* st, void* data, int length);
int sx_tpop(xstack_t* st, void* data, int length);
int sx_empty(xstack_t* st);


int si_init(istack_t** st, size_t size);
void si_del(istack_t** st);
int si_resize(istack_t* st, int size);
void si_push(istack_t* st, int data );
void si_pop(istack_t* st);
void si_top(istack_t* st, int* data);
void si_tpop(istack_t* st, int* data);
int si_empty(istack_t* st);
int si_full(istack* st);
