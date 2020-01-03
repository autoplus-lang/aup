#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "object.h"
#include "memory.h"

static void resetStack(aupVM *vm)
{
	vm->top = vm->stack;
	vm->frameCount = 0;
	vm->openUpvalues = NULL;
}

static void runtimeError(aupVM *vm, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	for (int i = vm->frameCount - 1; i >= 0; i--) {
        aupCallFrame *frame = &vm->frames[i];
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

aupVM *aup_create(aupAlloc alloc)
{
    aupVM *vm = malloc(sizeof(aupVM));

    if (vm != NULL) {
        memset(vm, '\0', sizeof(aupVM));

        vm->numTempRoots = 0;
        vm->alloc = (alloc == NULL) ?
            aup_defaultAlloc : alloc;

        vm->objects = NULL;
        vm->bytesAllocated = 0;
        vm->nextGC = 1024 * 1024;

        vm->grayCount = 0;
        vm->grayCapacity = 0;
        vm->grayStack = NULL;
       
        aupT_init(&vm->globals);
        aupT_init(&vm->strings);       

        resetStack(vm);
    }

	return vm;
}

void aup_close(aupVM *vm)
{
    if (vm == NULL) return;

    aupT_free(&vm->globals);
    aupT_free(&vm->strings);
    aup_freeObjects(vm);

    free(vm);
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

    aupCallFrame *frame = &vm->frames[vm->frameCount++];
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

	runtimeError(vm, "Cannot perform call, got <%s>.", TOF(callee));
	return false;
}

static aupOu *captureUpvalue(AUP_VM, aupV *local)
{
	aupOu *prevUpvalue = NULL;
	aupOu *upvalue = vm->openUpvalues;

	while (upvalue != NULL && upvalue->value > local) {
		prevUpvalue = upvalue;
		upvalue = upvalue->next;
	}

	if (upvalue != NULL && upvalue->value == local) return upvalue;

	aupOu *createdUpvalue = aupOu_new(vm, local);
	createdUpvalue->next = upvalue;

	if (prevUpvalue == NULL) {
		vm->openUpvalues = createdUpvalue;
	}
	else {
		prevUpvalue->next = createdUpvalue;
	}

	return createdUpvalue;
}

static void closeUpvalues(AUP_VM, aupV *last)
{
	while (vm->openUpvalues != NULL && vm->openUpvalues->value >= last) {
		aupOu *upvalue = vm->openUpvalues;
		upvalue->closed = *upvalue->value;
		upvalue->value = &upvalue->closed;
		vm->openUpvalues = upvalue->next;
	}
}

static int exec(aupVM *vm)
{
	register uint32_t *ip;
	register aupCallFrame *frame;

#define STORE_FRAME() \
	frame->ip = ip

#define LOAD_FRAME() \
	frame = &vm->frames[vm->frameCount - 1]; \
	ip = frame->ip

#define ERROR(fmt, ...) \
    do { STORE_FRAME(); runtimeError(vm, fmt, ##__VA_ARGS__); return AUP_RUNTIME_ERROR; } while (0)

#define READ()		(ip[-1])
#define FETCH()		AUP_GET_Op(*ip++)

#define GET_Op()	AUP_GET_Op(READ())
#define GET_A()		AUP_GET_A(READ())
#define GET_Ax()	AUP_GET_Ax(READ())
#define GET_B()		AUP_GET_B(READ())
#define GET_C()		AUP_GET_C(READ())
#define GET_sB()	AUP_GET_sB(READ())
#define GET_sC()	AUP_GET_sC(READ())

#define R(i)		(frame->stack[i])
#define K(i)		(frame->function->chunk.constants.values[i])
#define U(i)        *(frame->function->upvalues[i]->value)

#define R_A()		R(GET_A())
#define R_B()		R(GET_B())
#define R_C()		R(GET_C())

#define K_A()		K(GET_A())
#define K_B()		K(GET_B())
#define K_C()		K(GET_C())

#define RK_B()		(GET_sB() ? K_B() : R_B())
#define RK_C()		(GET_sC() ? K_C() : R_C())

#define U_A()       U(GET_A())
#define U_B()       U(GET_B())

#if defined(__GNUC__) || defined(__clang__) || defined(__MINGW32__) || defined(__MINGW64__)
#define INTERPRET() NEXT;
#define CODE(x)     _AUP_OP_##x
#define CODE_ERR()  _err
#define NEXT        goto *_jtbl[FETCH()]
    static void *_jtbl[] = {
        &&CODE(NOP),
        &&CODE(CALL),   &&CODE(RET),
        &&CODE(PUSH),   &&CODE(POP),    &&CODE(PUT),
        &&CODE(NIL),    &&CODE(BOOL),
        &&CODE(NOT),    &&CODE(NEG),
        &&CODE(LT),     &&CODE(LE),     &&CODE(EQ),
        &&CODE(ADD),    &&CODE(SUB),    &&CODE(MUL),    &&CODE(DIV),    &&CODE(MOD),
        &&CODE(MOV),
        &&CODE(GLD),    &&CODE(GST),
        &&CODE(LD),     &&CODE(ST),
        &&CODE(CLU),    &&CODE(CLO),    &&CODE(ULD),    &&CODE(UST),
        &&CODE(JMP),    &&CODE(JMPF),
    };
#else
#define INTERPRET() _loop: switch (FETCH())
#define CODE(x)     case AUP_OP_##x
#define CODE_ERR()  default
#define NEXT        goto _loop
#endif

#define BINOP(op, fn) \
    aupV left = RK_B(); \
    aupV right = RK_C(); \
    switch (AUP_CMB(AUP_TYPE(left), AUP_TYPE(right))) { \
        case AUP_TINT_INT: \
            R_A() = AUP_INT(AUP_AS_INT(left) op AUP_AS_INT(right)); \
            NEXT; \
        case AUP_TNUM_NUM: \
            R_A() = AUP_NUM(AUP_AS_NUM(left) op AUP_AS_NUM(right)); \
            NEXT; \
        case AUP_TINT_NUM: \
            R_A() = AUP_NUM(AUP_AS_INT(left) op AUP_AS_NUM(right)); \
            NEXT; \
        case AUP_TNUM_INT: \
            R_A() = AUP_NUM(AUP_AS_NUM(left) op AUP_AS_INT(right)); \
            NEXT; \
    } \
    ERROR("Cannot perform '%s', got <%s> and <%s>.", #op, TOF(left), TOF(right))

#define BINOP_LG(op, fn) \
    aupV left = RK_B(); \
    aupV right = RK_C(); \
    switch (AUP_CMB(AUP_TYPE(left), AUP_TYPE(right))) { \
        case AUP_TINT_INT: \
            R_A() = AUP_BOOL(AUP_AS_INT(left) op AUP_AS_INT(right)); \
            NEXT; \
        case AUP_TNUM_NUM: \
            R_A() = AUP_BOOL(AUP_AS_NUM(left) op AUP_AS_NUM(right)); \
            NEXT; \
        case AUP_TINT_NUM: \
            R_A() = AUP_BOOL(AUP_AS_INT(left) op AUP_AS_NUM(right)); \
            NEXT; \
        case AUP_TNUM_INT: \
            R_A() = AUP_BOOL(AUP_AS_NUM(left) op AUP_AS_INT(right)); \
            NEXT; \
    } \
    ERROR("Cannot perform '%s', got <%s> and <%s>.", #op, TOF(left), TOF(right))

	LOAD_FRAME();

    INTERPRET()
    {
        CODE(PUSH):
        {
			PUSH(RK_B());
			NEXT;
		}

        CODE(POP):
        {
			POP();
			NEXT;
		}

		CODE(RET):
        {
			closeUpvalues(vm, frame->stack);
			if (--vm->frameCount == 0) {
				//pop();
				return AUP_OK;
			}
			//vm->top = frame->stack;
			R(0) = GET_A() ? RK_B() : AUP_NIL;
			LOAD_FRAME();
            NEXT;
        }

        CODE(CALL):
        {
            int argCount = GET_B();

            STORE_FRAME();
            //vm->top = &R_A();

            if (!callValue(vm, *(vm->top = &R_A()), argCount)) {
                return AUP_RUNTIME_ERROR;
            }

            LOAD_FRAME();
            NEXT;
        }

        CODE(PUT):
        {
            int rA = GET_A(), nvalues = GET_B();
            for (int i = 0; i < nvalues; i++) {
                aupV_print(R(rA + i));
                if (i < nvalues - 1) printf("\t");
            }
            printf("\n");
            fflush(stdout);
            NEXT;
        }

        CODE(MOV):
        {
            R_A() = R_B();
            NEXT;
        }

        CODE(NIL):
        {
            R_A() = AUP_NIL;
            NEXT;
        }

        CODE(BOOL):
        {
            R_A() = AUP_BOOL(GET_sB());
            NEXT;
        }

        CODE(NOT):
        {
            R_A() = AUP_BOOL(AUP_IS_FALSEY(RK_B()));
            NEXT;
        }

        CODE(NEG):
        {
            aupV value = RK_B();
            switch (AUP_TYPE(value)) {
                case AUP_TINT:
                    R_A() = AUP_INT(-AUP_AS_INT(value));
                    NEXT;
                case AUP_TNUM:
                    R_A() = AUP_NUM(-AUP_AS_NUM(value));
                    NEXT;
            }
            ERROR("Cannot perform '-', got <%s>.", TOF(value));
        }

        CODE(LT):
        {
            BINOP_LG( < , lt);
            NEXT;
        }

        CODE(LE):
        {
            BINOP_LG( <= , lt);
            NEXT;
        }

        CODE(EQ):
        {
            BINOP_LG( == , lt);
            NEXT;
        }

        CODE(ADD):
        {
            BINOP( + , add);
            NEXT;
        }

        CODE(SUB):
        {
            BINOP( - , sub);
            NEXT;
        }

        CODE(MUL):
        {
            BINOP( * , mul);
            NEXT;
        }

        CODE(DIV):
        {
            BINOP( / , div);
            NEXT;
        }

        CODE(MOD):
        {
            aupV left = RK_B(), right = RK_C();
            switch (AUP_CMB(AUP_TYPE(left), AUP_TYPE(right))) {
                case AUP_TINT_INT:
                    R_A() = AUP_INT(AUP_AS_INT(left) % AUP_AS_INT(right));
                    NEXT;
                case AUP_TNUM_NUM:
                    R_A() = AUP_INT((int64_t)AUP_AS_NUM(left) % (int64_t)AUP_AS_NUM(right));
                    NEXT;
                case AUP_TINT_NUM:
                    R_A() = AUP_INT(AUP_AS_INT(left) % (int64_t)AUP_AS_NUM(right));
                    NEXT;
                case AUP_TNUM_INT:
                    R_A() = AUP_INT((int64_t)AUP_AS_NUM(left) % AUP_AS_INT(right));
                    NEXT;
            }
            ERROR("Cannot perform '%', got <%s> and <%s>.", TOF(left), TOF(right));
        }

        CODE(GLD):
        {
            aupOs *name = AUP_AS_STR(K_B());
            if (!aupT_get(&vm->globals, name, &R_A())) {
                R_A() = AUP_NIL;
            }
            NEXT;
        }

        CODE(GST):
        {
            aupOs *name = AUP_AS_STR(K_A());
            aupT_set(&vm->globals, name, GET_sC() ? AUP_NIL : RK_B());
            NEXT;
        }

        CODE(LD):
        {
            R_A() = RK_B();
            NEXT;
        }

        CODE(ST):
        {
            R_A() = R_B();
            NEXT;
        }

        CODE(CLOSE):
        {
            closeUpvalues(vm, frame->stack + GET_A());
            NEXT;
        }

        CODE(CLOSURE):
        {
            aupOf *function = AUP_AS_FUN(K_A());
            aupOf_closure(function);

            for (int i = 0; i < function->upvalueCount; i++) {
                FETCH();
                if (GET_sB()) {
                    function->upvalues[i] = captureUpvalue(vm, frame->stack + GET_A());
                }
                else {
                    function->upvalues[i] = frame->function->upvalues[GET_A()];
                }
            }
            NEXT;
        }

        CODE(ULD):
        {
            R_A() = U_B();
            NEXT;
        }

        CODE(UST):
        {
            U_A() = RK_B();
            NEXT;
        }

        CODE(JMP):
        {
            ip += GET_Ax();
            NEXT;
        }

        CODE(JMPF):
        {
            aupV value = R_C();
            if (AUP_IS_FALSEY(value)) ip += GET_Ax();
            NEXT;
        }

        CODE_ERR():
        {
            ERROR("Bad opcode, got %d.", GET_Op());
		}
	}

	return AUP_OK;
}

int aup_doString(aupVM *vm, const char *source)
{
	aupOf *function = aup_compile(vm, source);
	if (function == NULL) return AUP_COMPILE_ERROR;

	aupV script = AUP_OBJ(function);
	PUSH(script); POP();
	callValue(vm, script, 0);

	return exec(vm);
}

int aup_doFile(aupVM *vm, const char *name)
{
    if (vm == NULL) return AUP_INVALID;

    char *source = aup_readFile(name, NULL);

    if (source != NULL) {
        int ret = aup_doString(vm, source);
        free(source);
        return ret;
    }

    return AUP_COMPILE_ERROR;
}

void aup_pushRoot(aupVM *vm, aupO *object)
{
    vm->tempRoots[vm->numTempRoots++] = object;
}

void aup_popRoot(aupVM *vm)
{
    vm->numTempRoots--;
}
