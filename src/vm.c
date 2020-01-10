#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "vm.h"
#include "code.h"
#include "object.h"
#include "table.h"
#include "gc.h"

static void resetStack(aupVM *vm)
{
    vm->top = vm->stack;
    vm->frameCount = 0;
    vm->openUpvalues = NULL;
}

static void runtimeError(aupVM *vm, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm->frameCount - 1; i >= 0; i--) {
        aupFrame *frame = &vm->frames[i];
        aupFun *function = frame->function;
        // -1 because the IP is sitting on the next instruction to be
        // executed.                                                 
        size_t instruction = frame->ip - function->chunk.code - 1;
        const char *fname = frame->function->chunk.source->fname;
        int line = frame->function->chunk.lines[instruction];
        int column = frame->function->chunk.columns[instruction];
        fprintf(stderr, "[%s:%d:%d] in ", fname, line, column);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        }
        else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    fflush(stderr);
    resetStack(vm);
}

aupVM *aup_create()
{
    aupVM *vm = malloc(sizeof(aupVM));
    if (vm == NULL) return NULL;

    memset(vm->stack, '\0', sizeof(vm->stack));
    memset(vm->frames, '\0', sizeof(vm->frames));

    vm->gc = malloc(sizeof(aupGC));
    vm->globals = malloc(sizeof(aupTab));
    vm->strings = malloc(sizeof(aupTab));

    vm->errmsg = NULL;
    vm->hadError = false;

    aup_initGC(vm->gc);
    aup_initTable(vm->globals);
    aup_initTable(vm->strings);

    resetStack(vm);
    return vm;
}

void aup_close(aupVM *vm)
{
    if (vm == NULL) return;

    aup_freeTable(vm->globals);
    aup_freeTable(vm->strings);
    aup_freeGC(vm->gc);

    free(vm->globals);
    free(vm->strings);
    free(vm->gc);

    free(vm->errmsg);

    free(vm);
}

aupVM *aup_cloneVM(aupVM *from)
{
    aupVM *vm = malloc(sizeof(aupVM));
    if (vm == NULL) return NULL;

    memset(vm, '\0', sizeof(aupVM));

    vm->gc = from->gc;
    vm->globals = from->globals;
    vm->strings = from->strings;

    resetStack(vm);
    return vm;
}

#define PUSH(v)     *((vm)->top++) = (v)
#define POP()       *(--(vm)->top)
#define POPN(n)     *((vm)->top -= (n))
#define PEEK(i)     ((vm)->top[-1 - (i)])

static void concatenate(aupVM *vm)
{
    aupStr *b = AUP_AS_STR(POP());
    aupStr *a = AUP_AS_STR(POP());

    int length = a->length + b->length;
    char *chars = malloc((length + 1) * sizeof(char));
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    aupStr *result = aup_takeString(vm, chars, length);
    PUSH(AUP_OBJ(result));
}

static bool prepareCall(aupVM *vm, aupFun *function, int argCount)
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

    aupFrame *frame = &vm->frames[vm->frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;

    frame->slots = vm->top - argCount - 1;
    return true;
}

bool aup_call(aupVM *vm, aupVal callee, int argCount)
{
    if (AUP_IS_OBJ(callee)) {
        switch (AUP_OBJTYPE(callee)) {
            case AUP_TFUN:
                return prepareCall(vm, AUP_AS_FUN(callee), argCount);

            default:
                // Non-callable object type.                   
                break;
        }
    }
    else if (AUP_IS_CFN(callee)) {
        aupCFn native = AUP_AS_CFN(callee);
        aupVal result = native(vm, argCount, vm->top - argCount);
        if (vm->hadError) {
            runtimeError(vm, "%s", vm->errmsg);
            return false;
        }
        vm->top -= argCount + 1;
        PUSH(result);
        return true;
    }

    runtimeError(vm, "Can only call functions and classes.");
    return false;
}

static aupUpv *captureUpvalue(aupVM *vm, aupVal *local)
{
    aupUpv *prevUpvalue = NULL;
    aupUpv *upvalue = vm->openUpvalues;

    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) return upvalue;

    aupUpv *createdUpvalue = aup_newUpvalue(vm, local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        vm->openUpvalues = createdUpvalue;
    }
    else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(aupVM *vm, aupVal *last)
{
    while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
        aupUpv *upvalue = vm->openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->openUpvalues = upvalue->next;
    }
}

int aup_execute(aupVM *vm)
{
    register uint8_t *ip;
    register aupVal *stack;
    register aupVal *consts;
    register aupFrame *frame;

#define STORE_FRAME() \
    frame->ip = ip

#define LOAD_FRAME() \
    frame = &vm->frames[vm->frameCount - 1]; \
	ip = frame->ip; \
    stack = frame->slots; \
    consts = frame->function->chunk.constants.values

#define STACK           (stack)
#define CONSTS          (consts)

#define PREV_BYTE()     (ip[-1])
#define READ_BYTE()     *(ip++)
#define READ_SHORT()    (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

#define READ_CONST()    CONSTS[READ_BYTE()]
#define READ_STR()      AUP_AS_STR(READ_CONST())

#define ERROR(fmt, ...) \
    do { \
        STORE_FRAME(); \
        runtimeError(vm, fmt, ##__VA_ARGS__); \
        return AUP_RUNTIME_ERROR; \
    } while (0)

#ifdef _MSC_VER
// Never try the 'computed goto' below on MSVC x86!
#if 0 //defined(_M_IX86) || (defined(_WIN32) && !defined(_WIN64))
#define INTERPRET       NEXT;
#define CODE(x)         _OP_##x:
#define CODE_ERR()      
#define NEXT            do { size_t i = READ_BYTE() * sizeof(size_t); __asm {mov ecx, [i]} __asm {jmp _jtab[ecx]} } while (0)
    static size_t _jtab[OPCODE_COUNT];
    if (_jtab[0] == 0) {
#define _CODE(x) __asm { mov _jtab[TYPE _jtab * OP_##x], offset _OP_##x }
        OPCODES();
#undef _CODE
    }
#else
#define INTERPRET       _loop: switch(READ_BYTE())
#define CODE(x)         case AUP_OP_##x:
#define CODE_ERR()      default:
#define NEXT            goto _loop
#endif
#else
#define INTERPRET       NEXT;
#define CODE(x)         _AUP_OP_##x:
#define CODE_ERR()      _err:
#define NEXT            goto *_jtab[READ_BYTE()]
#define _CODE(x)        &&_AUP_OP_##x,
    static void *_jtab[AUP_OPCOUNT] = { OPCODES() };
#endif

    LOAD_FRAME();

    INTERPRET
    {
        CODE(PRINT) {
            int count = READ_BYTE();

            for (int i = count-1; i >= 0; i--) {
                aup_printValue(PEEK(i));
                if (i > 0) printf("\t");
            }
            printf("\n");

            POPN(count);
            NEXT;
        }

        CODE(POP) {
            POP();
            NEXT;
        }

        CODE(NIL) {
            PUSH(AUP_NIL);
            NEXT;
        }

        CODE(TRUE) {
            PUSH(AUP_TRUE);
            NEXT;
        }

        CODE(FALSE) {
            PUSH(AUP_FALSE);
            NEXT;
        }

        CODE(CONST) {
            PUSH(READ_CONST());
            NEXT;
        }

        CODE(CALL) {
            int argCount = READ_BYTE();

            STORE_FRAME();
            if (!aup_call(vm, PEEK(argCount), argCount)) {
                return AUP_RUNTIME_ERROR;
            }

            LOAD_FRAME();
            NEXT;
        }

        CODE(RET) {
            aupVal result = POP();
            closeUpvalues(vm, frame->slots);

            if (--vm->frameCount == 0) {
                POP();
                return AUP_OK;
            }

            vm->top = frame->slots;
            PUSH(result);

            LOAD_FRAME();
            NEXT;
        }

        CODE(NOT) {
            PUSH(AUP_BOOL(AUP_IS_FALSEY(POP())));
            NEXT;
        }

        CODE(NEG) {
            switch (PEEK(0).type) {
                case AUP_TBOOL:
                    PUSH(AUP_NUM(-(char)AUP_AS_BOOL(POP())));
                    NEXT;
                case AUP_TNUM:
                    PUSH(AUP_NUM(-AUP_AS_NUM(POP())));
                    NEXT;
            }
            ERROR("Operands must be a number/boolean.");
        }

        CODE(EQ) {
            aupVal b = POP();
            aupVal a = POP();
            PUSH(AUP_BOOL(aup_valuesEqual(a, b)));
            NEXT;
        }

        CODE(LT) {
            if (AUP_IS_NUM(PEEK(1)) && AUP_IS_NUM(PEEK(0))) {
                double b = AUP_AS_NUM(POP());
                double a = AUP_AS_NUM(POP());
                PUSH(AUP_BOOL(a < b));
                NEXT;
            }
            ERROR("Operands must be two numbers.");
        }

        CODE(LE) {
            if (AUP_IS_NUM(PEEK(1)) && AUP_IS_NUM(PEEK(0))) {
                double b = AUP_AS_NUM(POP());
                double a = AUP_AS_NUM(POP());
                PUSH(AUP_BOOL(a <= b));
                NEXT;
            }
            ERROR("Operands must be two numbers.");
        }

        CODE(ADD) {
            if (AUP_IS_NUM(PEEK(1)) && AUP_IS_NUM(PEEK(0))) {
                double b = AUP_AS_NUM(POP());
                double a = AUP_AS_NUM(POP());
                PUSH(AUP_NUM(a + b));
                NEXT;
            }
            else if (AUP_IS_STR(PEEK(1)) && AUP_IS_STR(PEEK(0))) {
                concatenate(vm);
                NEXT;
            }
            ERROR("Operands must be two numbers or strings.");
        }

        CODE(SUB) {
            if (AUP_IS_NUM(PEEK(1)) && AUP_IS_NUM(PEEK(0))) {
                double b = AUP_AS_NUM(POP());
                double a = AUP_AS_NUM(POP());
                PUSH(AUP_NUM(a - b));
                NEXT;
            }
            ERROR("Operands must be two numbers.");
        }

        CODE(MUL) {
            if (AUP_IS_NUM(PEEK(1)) && AUP_IS_NUM(PEEK(0))) {
                double b = AUP_AS_NUM(POP());
                double a = AUP_AS_NUM(POP());
                PUSH(AUP_NUM(a * b));
                NEXT;
            }
            ERROR("Operands must be two numbers.");
        }

        CODE(DIV) {
            if (AUP_IS_NUM(PEEK(1)) && AUP_IS_NUM(PEEK(0))) {
                double b = AUP_AS_NUM(POP());
                double a = AUP_AS_NUM(POP());
                PUSH(AUP_NUM(a / b));
                NEXT;
            }
            ERROR("Operands must be two numbers.");
        }

        CODE(MOD) {
            if (AUP_IS_NUM(PEEK(1)) && AUP_IS_NUM(PEEK(0))) {
                int64_t b = AUP_AS_INT64(POP());
                int64_t a = AUP_AS_INT64(POP());
                PUSH(AUP_NUM((double)(a % b)));
                NEXT;
            }
            ERROR("Operands must be two numbers.");
        }

        CODE(DEF) {
            aupStr *name = READ_STR();
            aup_setTable(vm->globals, name, PEEK(0));
            POP();
            NEXT;
        }

        CODE(GLD) {
            aupStr *name = READ_STR();
            aupVal value = AUP_NIL;      
            aup_getTable(vm->globals, name, &value);
            PUSH(value);
            NEXT;
        }

        CODE(GST) {
            aupStr *name = READ_STR();
            aup_setTable(vm->globals, name, PEEK(0));
            NEXT;
        }

        CODE(LD) {
            PUSH(STACK[READ_BYTE()]);
            NEXT;
        }

        CODE(ST) {
            STACK[READ_BYTE()] = PEEK(0);
            NEXT;
        }

        CODE(JMP) {
            uint16_t offset = READ_SHORT();
            ip += offset;
            NEXT;
        }

        CODE(JMPF) {
            uint16_t offset = READ_SHORT();
            if (AUP_IS_FALSEY(PEEK(0))) ip += offset;
            NEXT;
        }

        CODE(MAP) {
            uint8_t count = READ_BYTE();
            aupMap *map = aup_newMap(vm);

            for (aupVal i = AUP_NUM(count - 1); AUP_AS_NUM(i) >= 0; AUP_AS_NUM(i)--) {
                aup_setHash(&map->hash, AUP_AS_RAW(i), PEEK((int)AUP_AS_NUM(i)));
            }

            POPN(count);
            PUSH(AUP_OBJ(map));
            NEXT;
        }

        CODE(GET) {
            if (AUP_IS_MAP(PEEK(0))) {
                aupMap *map = AUP_AS_MAP(PEEK(0));
                aupStr *name = READ_STR();
                aupVal value = AUP_NIL;
                aup_getTable(&map->table, name, &value);
                POP();
                PUSH(value);
            }
            else {
                ERROR("Operands must be a map.");
            }
            NEXT;
        }

        CODE(SET) {
            if (AUP_IS_MAP(PEEK(1))) {
                aupMap *map = AUP_AS_MAP(PEEK(1));
                aupStr *name = READ_STR();
                aupVal value = PEEK(0);
                aup_setTable(&map->table, name, value);
                POP();
                POP();
                PUSH(value);
            }
            else {
                ERROR("Operands must be a map.");
            }
            NEXT;
        }

        CODE(GETI) {
            if (AUP_IS_MAP(PEEK(1))) {
                if (AUP_IS_NUM(PEEK(0))) {
                    aupMap *map = AUP_AS_MAP(PEEK(1));
                    uint64_t key = AUP_AS_RAW(PEEK(0));
                    aupVal value = AUP_NIL;
                    aup_getHash(&map->hash, key, &value);

                    POP();
                    POP();
                    PUSH(value);
                }
                else if (AUP_IS_STR(PEEK(0))) {
                    aupMap *map = AUP_AS_MAP(PEEK(1));
                    aupStr *key = AUP_AS_STR(PEEK(0));
                    aupVal value = AUP_NIL;
                    aup_getTable(&map->table, key, &value);

                    POP();
                    POP();
                    PUSH(value);
                }
                else {
                    ERROR("Operands must be a number or string.");
                }
            }
            else {
                ERROR("Operands must be a map.");
            }
            NEXT;
        }

        CODE(SETI) {
            if (AUP_IS_MAP(PEEK(2))) {
                if (AUP_IS_NUM(PEEK(1))) {
                    aupMap *map = AUP_AS_MAP(PEEK(2));
                    uint64_t key = AUP_AS_RAW(PEEK(1));
                    aupVal value = POP();
                    aup_setHash(&map->hash, key, value);

                    POP();
                    POP();
                    PUSH(value);
                }
                else if (AUP_IS_STR(PEEK(1)))
                {
                    aupMap *map = AUP_AS_MAP(PEEK(2));
                    aupStr *key = AUP_AS_STR(PEEK(1));
                    aupVal value = POP();
                    aup_setTable(&map->table, key, value);

                    POP();
                    POP();
                    PUSH(value);
                }
                else {
                    ERROR("Operands must be a number or string.");
                }
            }
            else {
                ERROR("Operands must be a map.");
            }
            NEXT;
        }

        CODE(CLOSURE) {
            aupFun *function = AUP_AS_FUN(READ_CONST());
            aup_makeClosure(function);

            for (int i = 0; i < function->upvalueCount; i++) {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal) {
                    function->upvalues[i] = captureUpvalue(vm, frame->slots + index);
                }
                else {
                    function->upvalues[i] = frame->function->upvalues[index];
                }
            }

            NEXT;
        }

        CODE(CLOSE) {
            closeUpvalues(vm, vm->top - 1);
            POP();
            NEXT;
        }

        CODE(ULD) {
            uint8_t slot = READ_BYTE();
            PUSH(*frame->function->upvalues[slot]->location);
            NEXT;
        }

        CODE(UST) {
            uint8_t slot = READ_BYTE();
            *frame->function->upvalues[slot]->location = PEEK(0);
            NEXT;
        }

        CODE_ERR() {
            ERROR("Bad opcode, got %d!", PREV_BYTE());
        }
    }

    return AUP_OK;
}

int aup_doFile(aupVM *vm, const char *fname)
{
    int result = AUP_COMPILE_ERROR;
    aupSrc *source = aup_newSource(fname);

    if (source != NULL) {
        aupFun *function = aup_compile(vm, source);
        if (function == NULL) return AUP_COMPILE_ERROR;

        aupVal script = AUP_OBJ(function);

        PUSH(script);
        aup_call(vm, script, 0);

        result = aup_execute(vm);  
    }

    aup_freeSource(source);
    return result;
}

aupVal aup_error(aupVM *vm, const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    int len = vsprintf(NULL, msg, ap);
    char *buf = malloc((len + 1) * sizeof(char));
    vsprintf(buf, msg, ap);
    va_end(ap);

    buf[len] = '\0';
    vm->hadError = true;
    vm->errmsg = buf;

    return AUP_NIL;
}

void aup_push(aupVM *vm, aupVal value)
{
    if (vm->hadError) return;
    PUSH(value);
}

void aup_pop(aupVM *vm)
{
    if (vm->hadError) return;
    POP();
}

void aup_pushRoot(aupVM *vm, aupObj *object)
{
    vm->tempRoots[vm->numRoots++] = object;
}

void aup_popRoot(aupVM *vm)
{
    vm->numRoots--;
}
