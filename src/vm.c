#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "object.h"
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

#define GET_Op()	AUP_GET_Op(i)
#define GET_A()		AUP_GET_A(i)
#define GET_Ax()	AUP_GET_Ax(i)
#define GET_B()		AUP_GET_B(i)
#define GET_C()		AUP_GET_C(i)
#define GET_sB()	AUP_GET_sB(i)
#define GET_sC()	AUP_GET_sC(i)

#define REG(i)		(vm->stack[i])
#define REG_K(i)	(vm->chunk->constants.values[i])

#define REG_A()		REG(GET_A())
#define REG_B()		REG(GET_B())
#define REG_C()		REG(GET_C())

#define REG_BK()	(GET_sB() ? REG_K(GET_B()) : REG(GET_B()))
#define REG_CK()	(GET_sC() ? REG_K(GET_C()) : REG(GET_C()))

#define dispatch()	for (;;) switch (AUP_GET_Op(i = *(vm->ip++)))
#define code(x)		case (AUP_OP_##x):
#define code_err()	default:
#define next		continue

	dispatch() {
		code(NOP) next;

		code(RET) {

			return AUP_OK;
		}

		code(PUT) {
			aupV_print(REG_A());
			printf("\n");
			next;
		}

		code(LDK) {
			REG_A() = REG_K(GET_B());
			next;
		}

		code(NOT) {
			aupV value = REG(GET_B());
			REG_A() = AUP_BOOL(AUP_IS_FALSE(value));
			next;
		}

		code(NEG) {
			aupV value = REG(GET_B());
			if (AUP_IS_NUM(value)) {
				REG_A() = AUP_NUM(-AUP_AS_NUM(value));
			}
			else {
				runtimeError(vm, "cannot perform '-', got <%s>.", aupV_typeOf(value));
				return AUP_RUNTIME_ERR;
			}
			next;
		}

		code(ADD) {
			aupV left = REG(GET_B()), right = REG(GET_C());
			if (AUP_IS_NUM(left) && AUP_IS_NUM(right)) {
				REG_A() = AUP_NUM(AUP_AS_NUM(left) + AUP_AS_NUM(right));
			}
			else {
				runtimeError(vm, "cannot perform '+', got <%s> and <%s>.", aupV_typeOf(left), aupV_typeOf(right));
				return AUP_RUNTIME_ERR;
			}
			next;
		}

		code(DEF) {
			aupOs *name = AUP_AS_STR(REG_K(GET_A()));
			aupT_set(&vm->globals, name, GET_sB() ? AUP_NIL : REG(GET_B()));
			next;
		}
		code(GLD) {
			aupOs *name = AUP_AS_STR(REG_K(GET_B()));
			if (!aupT_get(&vm->globals, name, &REG_A())) {
				runtimeError(vm, "undefined variable '%s'.", name->chars);
				return AUP_RUNTIME_ERR;
			}
			next;
		}
		code(GST) {
			aupOs *name = AUP_AS_STR(REG_K(GET_A()));
			if (aupT_set(&vm->globals, name, REG(GET_B()))) {
				aupT_delete(&vm->globals, name);
				runtimeError(vm, "undefined variable '%s'.", name->chars);
				return AUP_RUNTIME_ERR;
			}
			next;
		}

		code_err() {
			runtimeError(vm, "bad opcode, got %d.", AUP_GET_Op(i));
			return AUP_RUNTIME_ERR;
		}
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
