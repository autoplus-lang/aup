#include <stdio.h>

#include "aup.h"
#include "value.h"



void printV(aupV v)
{
	printf("[%s] ", !aup_isFalsey(v) ? "true" : "false");

	if (AUP_IS_NIL(v))
		printf("nil\n");
	else if (AUP_IS_BOOL(v))
		printf("bool: %s\n", AUP_AS_BOOL(v) ? "true" : "false");
	else if (AUP_IS_NUM(v))
		printf("num: %.14g\n", AUP_AS_NUM(v));
	else if (AUP_IS_OBJ(v))
		printf("obj: %p\n", AUP_AS_OBJ(v));
	else
		printf("unknow\n");
}

int main(int argc, char *argv[])
{
	aupV v;

	v = AUP_NIL;
	printV(v);
	printf("%llX\n", v);

    v = AUP_TRUE;
	printV(v);
	printf("%llX\n", v);

	v = AUP_FALSE;
	printV(v);
	printf("%llX\n", v);

	v = AUP_NUM(0.0);
	printV(v);
	printf("%016llX\n", v);

	v = AUP_NUM(1);
	printV(v);
	printf("%llX\n", v);

	v = AUP_NUM(3.14);
	printV(v);
	printf("%llX\n", v);

	v = AUP_OBJ(NULL);
	printV(v);
	printf("%llX\n", v);

	v = AUP_OBJ(&main);
	printV(v);
	printf("%llX\n", v);
}