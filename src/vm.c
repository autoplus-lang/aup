#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "memory.h"

static aupVM g_vm, *g_pvm = NULL;

static void resetStack(aupVM *vm)
{
	vm->top = vm->stack;
}

static void runtimeError(aupVM *vm, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	size_t instruction = vm->ip - vm->chunk->code;
	int line = vm->chunk->lines[instruction];
	int column = vm->chunk->columns[instruction];
	fprintf(stderr, "[%d:%d] in script\n", line, column);

	resetStack(vm);
}

aupVM *aupVM_new()
{
	aupVM *vm;
	if (g_pvm == NULL) {
		vm = &g_vm;
		g_pvm = vm;
	}
	else {
		vm = malloc(sizeof(aupVM));
		if (vm == NULL) return NULL;
		memset(vm, '\0', sizeof(aupVM));
	}
	{
		vm->objects = NULL;
		aupT_init(&vm->globals);
		aupT_init(&vm->strings);
	}
	return vm;
}

void aupVM_free(aupVM *vm)
{
	if (vm == NULL && g_pvm != NULL)
		vm = g_pvm;
	{
		aupT_free(&vm->globals);
		aupT_free(&vm->strings);
		aup_freeObjects(vm);
	}
	if (vm != g_pvm) free(vm);
	else g_pvm = NULL;
}

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

int aup_interpret(aupVM *vm, const char *source)
{
	aupCh chunk;
	aupCh_init(&chunk);

	if (!aup_compile(vm, source, &chunk)) {
		aupCh_free(&chunk);
		return AUP_COMPILE_ERR;
	}

	vm->chunk = &chunk;
	vm->ip = vm->chunk->code;

	int result = exec(vm);

	aupCh_free(&chunk);
	return result;
}
