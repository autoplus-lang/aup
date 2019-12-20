#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"



static int exec(aupVM *vm)
{
	uint32_t i;

#define dispatch()	for (;;) switch (AUP_GET_Op(i = *(vm->ip++)))
#define code(x)		case (AUP_OP_##x):
#define code_err()	default:
#define next		continue

	dispatch() {
		code(NOP) next;
		code(LDK);
	}

	return AUP_OK;
}