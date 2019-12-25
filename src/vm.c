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
	vm->frameCount = 0;
}

static void runtimeError(aupVM *vm, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	for (int i = vm->frameCount - 1; i >= 0; i--) {
		aupCf *frame = &vm->frames[i];
		aupOf *function = frame->function;
		// -1 because the IP is sitting on the next instruction to be
		// executed.                                                 
		size_t instruction = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "[%d:%d] in ",
			function->chunk.lines[instruction], function->chunk.columns[instruction]);
		if (function->name == NULL) {
			fprintf(stderr, "script\n");
		}
		else {
			fprintf(stderr, "%s()\n", function->name->chars);
		}
	}

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
		resetStack(vm);
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

static bool call(AUP_VM, aupOf *function, int argCount)
{
	if (argCount != function->arity) {
		runtimeError(vm, "Expected %d arguments but got %d.",
			function->arity, argCount);
		return false;
	}

	if (vm->frameCount == AUP_MAX_FRAMES) {
		runtimeError(vm, "Stack overflow.");
		return false;
	}

	aupCf *frame = &vm->frames[vm->frameCount++];
	frame->function = function;
	frame->ip = function->chunk.code;

	frame->stack = vm->top;
	return true;
}

#define TOF(v)	aupV_typeOf(v)
#define PUSH(v)	(*((vm)->top++) = (v))
#define POP()   (--(vm)->top)

static bool callValue(AUP_VM, aupV callee, int argCount)
{
	if (AUP_IS_OBJ(callee)) {
		switch (AUP_OBJ_TYPE(callee)) {
			case AUP_TFUN:
				return call(vm, AUP_AS_FUN(callee), argCount);
			default:               
				break;
		}
	}

	runtimeError(vm, "cannot perform call, got <%s>.", TOF(callee));
	return false;
}

static aupOu *captureUpvalue(AUP_VM, aupV *local)
{
	aupOu *createdUpvalue = aupOu_new(vm, local);
	return createdUpvalue;
}

static int exec(aupVM *vm)
{
	register uint32_t *ip;
	register aupCf *frame;

#define STORE_FRAME() \
	frame->ip = ip

#define LOAD_FRAME() \
	frame = &vm->frames[vm->frameCount - 1]; \
	ip = frame->ip

#define READ()		(ip[-1])
#define EVAL()		AUP_GET_Op(*ip++)

#define GET_Op()	AUP_GET_Op(READ())
#define GET_A()		AUP_GET_A(READ())
#define GET_Ax()	AUP_GET_Ax(READ())
#define GET_B()		AUP_GET_B(READ())
#define GET_C()		AUP_GET_C(READ())
#define GET_sB()	AUP_GET_sB(READ())
#define GET_sC()	AUP_GET_sC(READ())

#define R(i)		(frame->stack[i])
#define K(i)		(frame->function->chunk.constants.values[i])

#define R_A()		R(GET_A())
#define R_B()		R(GET_B())
#define R_C()		R(GET_C())

#define K_A()		K(GET_A())
#define K_B()		K(GET_B())
#define K_C()		K(GET_C())

#define RK_B()		(GET_sB() ? K_B() : R_B())
#define RK_C()		(GET_sC() ? K_C() : R_C())

#define dispatch()	for (;;) switch (EVAL())
#define code(x)		case (AUP_OP_##x):
#define code_err()	default:
#define next		continue

	LOAD_FRAME();

	dispatch() {
		code(NOP) {
			next;
		}

		code(PUSH) {
			PUSH(RK_B());
			next;
		}
		code(POP) {
			POP();
			next;
		}

		code(RET) {
			if (--vm->frameCount == 0) {
				//pop();
				return AUP_OK;
			}
			//vm->top = frame->stack;
			R(0) = GET_A() ? RK_B() : AUP_NIL;
			LOAD_FRAME();
			next;
		}
		code(CALL) {
			int argCount = GET_B();

			STORE_FRAME();
			//vm->top = &R_A();

			if (!callValue(vm, *(vm->top = &R_A()), argCount)) {
				return AUP_RUNTIME_ERR;
			}
			
			LOAD_FRAME();
			next;
		}

		code(PUT) {
			int rA = GET_A(), nvalues = GET_B();
			for (int i = 0; i < nvalues; i++) {
				aupV_print(R(rA + i));
				if (i < nvalues - 1) printf("\t");
			}
			printf("\n");
			fflush(stdout);
			next;
		}

		code(MOV) {
			R_A() = R_B();
			next;
		}

		code(NIL) {
			R_A() = AUP_NIL;
			next;
		}
		code(BOOL) {
			R_A() = AUP_BOOL(GET_sB());
			next;
		}

		code(NOT) {
			aupV value = RK_B();
			R_A() = AUP_BOOL(AUP_IS_FALSEY(value));
			next;
		}
		code(NEG) {
			aupV value = RK_B();
			if (AUP_IS_INT(value)) {
				R_A() = AUP_INT(-AUP_AS_INT(value));
			}
			else {
				runtimeError(vm, "cannot perform '-', got <%s>.", TOF(value));
				return AUP_RUNTIME_ERR;
			}
			next;
		}

		code(LT) {
			aupV left = RK_B(), right = RK_C();
			if (AUP_IS_INT(left) && AUP_IS_INT(right)) {
				R_A() = AUP_BOOL(AUP_AS_INT(left) < AUP_AS_INT(right));
			}
			else {
				runtimeError(vm, "cannot perform '<', got <%s> and <%s>.", TOF(left), TOF(right));
				return AUP_RUNTIME_ERR;
			}
			next;
		}
		code(LE) {
			aupV left = RK_B(), right = RK_C();
			if (AUP_IS_INT(left) && AUP_IS_INT(right)) {
				R_A() = AUP_BOOL(AUP_AS_INT(left) <= AUP_AS_INT(right));
			}
			else {
				runtimeError(vm, "cannot perform '<=', got <%s> and <%s>.", TOF(left), TOF(right));
				return AUP_RUNTIME_ERR;
			}
			next;
		}
		code(EQ) {
			aupV left = RK_B(), right = RK_C();
			if (AUP_IS_INT(left) && AUP_IS_INT(right)) {
				R_A() = AUP_BOOL(AUP_AS_INT(left) == AUP_AS_INT(right));
			}
			else {
				runtimeError(vm, "cannot perform '==', got <%s> and <%s>.", TOF(left), TOF(right));
				return AUP_RUNTIME_ERR;
			}
			next;
		}

		code(ADD) {
			aupV left = RK_B(), right = RK_C();
			if (AUP_IS_INT(left) && AUP_IS_INT(right)) {
				R_A() = AUP_INT(AUP_AS_INT(left) + AUP_AS_INT(right));
			}
			else {
				runtimeError(vm, "cannot perform '+', got <%s> and <%s>.", TOF(left), TOF(right));
				return AUP_RUNTIME_ERR;
			}
			next;
		}
		code(SUB) {
			aupV left = RK_B(), right = RK_C();
			if (AUP_IS_INT(left) && AUP_IS_INT(right)) {
				R_A() = AUP_INT(AUP_AS_INT(left) - AUP_AS_INT(right));
			}
			else {
				runtimeError(vm, "cannot perform '-', got <%s> and <%s>.", TOF(left), TOF(right));
				return AUP_RUNTIME_ERR;
			}
			next;
		}
		code(MUL) {
			aupV left = RK_B(), right = RK_C();
			if (AUP_IS_INT(left) && AUP_IS_INT(right)) {
				R_A() = AUP_INT(AUP_AS_INT(left) * AUP_AS_INT(right));
			}
			else {
				runtimeError(vm, "cannot perform '*', got <%s> and <%s>.", TOF(left), TOF(right));
				return AUP_RUNTIME_ERR;
			}
			next;
		}
		code(DIV) {
			aupV left = RK_B(), right = RK_C();
			if (AUP_IS_INT(left) && AUP_IS_INT(right)) {
				R_A() = AUP_INT(AUP_AS_INT(left) / AUP_AS_INT(right));
			}
			else {
				runtimeError(vm, "cannot perform '/', got <%s> and <%s>.", TOF(left), TOF(right));
				return AUP_RUNTIME_ERR;
			}
			next;
		}
		code(MOD) {
			aupV left = RK_B(), right = RK_C();
			if (AUP_IS_INT(left) && AUP_IS_INT(right)) {
				R_A() = AUP_INT((long)AUP_AS_INT(left) % (long)AUP_AS_INT(right));
			}
			else {
				runtimeError(vm, "cannot perform '%', got <%s> and <%s>.", TOF(left), TOF(right));
				return AUP_RUNTIME_ERR;
			}
			next;
		}

		code(DEF) {
			aupOs *name = AUP_AS_STR(K_A());
			aupT_set(&vm->globals, name, GET_sC() ? AUP_NIL : RK_B());
			next;
		}
		code(GLD) {
			aupOs *name = AUP_AS_STR(K_B());
			if (!aupT_get(&vm->globals, name, &R_A())) {
				R_A() = AUP_NIL;
				//runtimeError(vm, "undefined variable '%s'.", name->chars);
				//return AUP_RUNTIME_ERR;
			}
			next;
		}
		code(GST) {
			aupOs *name = AUP_AS_STR(K_A());
			if (aupT_set(&vm->globals, name, RK_B())) {
				//aupT_delete(&vm->globals, name);
				//runtimeError(vm, "undefined variable '%s'.", name->chars);
				//return AUP_RUNTIME_ERR;
			}
			next;
		}

		code(LD) {
			R_A() = RK_B();
			next;
		}
		code(ST) {
			R_A() = R_B();
			next;
		}

		code(CLO) {
			aupOf *function = AUP_AS_FUN(K_A());
			aupOf_closure(function);

			for (int i = 0; i < function->upvalueCount; i++) {
				EVAL(); int index = GET_A();
				if (GET_sB()) {
					function->upvalues[i] = captureUpvalue(vm, frame->stack + index);
				}
				else {
					function->upvalues[i] = frame->function->upvalues[index];
				}
			}
			next;
		}
		code(ULD) {
			R_A() = *frame->function->upvalues[GET_B()]->value;
			next;
		}
		code(UST) {
			*frame->function->upvalues[GET_A()]->value = R_B();
			next;
		}

		code(JMP) {
			ip += GET_Ax();
			next;
		}
		code(JMPF) {
			aupV value = R_C();
			if (AUP_IS_FALSEY(value)) ip += GET_Ax();
			next;
		}

		code_err() {
			runtimeError(vm, "bad opcode, got %d.", GET_Op());
			return AUP_RUNTIME_ERR;
		}
	}

	return AUP_OK;
}

int aup_interpret(aupVM *vm, const char *source)
{
	aupOf *function = aup_compile(vm, source);
	if (function == NULL) return AUP_COMPILE_ERR;

	aupV script = AUP_OBJ(function);
	PUSH(script); POP();
	callValue(vm, script, 0);

	return exec(vm);
}
