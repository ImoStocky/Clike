#ifndef IFJ_TYPES_H
#define IFJ_TYPES_H

typedef enum errv 
{
	comp_ok = 0,
	lex_err = 1,
	syn_err,
	sem_err,
	sem_type_err,
	amb_type_err,
	sem_other_err,
	rt_num_err,
	rt_no_init_err,
	rt_zdiv_err,
	rt_other_err,
	inter_err = 99
}errv_t;

typedef enum tokens
{
	keyword_tk = 1,
	int_tk,
	real_tk,
	literal_tk,
	operator_tk,
	ident_tk,
	semicolon_tk,
	unknown_tk
}token_t;





#define Galloc(size, ptr) ( (ptr)=malloc(size) != NULL ? 0 : exit(99), 1)





#endif //IFJ_TYPES_H

