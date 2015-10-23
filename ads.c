

int xstack_init(stack_t** arr, size_t size, size_t elem)
{

}

int xstack_resize(stack_t** arr, int n_size)
{
}

int xstack_push(xstack_t* arr, void* data, int length)
{
}

int xstack_pop(xstack_t* arr, int length)
{
}

int xstack_top(xstack_t* arr, void* data, int length)
{	
}

bool xstack_empty(xstack_t* arr)
{	
	return(arr->top < 0);
}

int xtable_init(xtable_t** arr, size_t num, size_t elem)
{
}

int xtable_insert(xtable_t* arr, int key, void* data)
{
}

bool xtable_search(xtable_t* arr, int key)
{
}

int xtable_delete(xtable_t* arr, int key)
{
}

int xtable_copy(xtable_t* arr, int key, void* data)
{
}

int xtable_resize(xtable_t** arr, size_t num, size_t elem)
{
}
