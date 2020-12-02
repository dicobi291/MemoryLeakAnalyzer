#include "header.hpp"
void foo(int *v);


int alo(int argc, char *argv[])
{
	print();
	int *a = new int;
	int *b;
	int *var = b = a;
	
	foo(var);
	
	*var = 5;
	
	return 0;
}

void foo(int *v)
{
	*v = 10;
}
