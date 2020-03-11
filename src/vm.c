#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "util.h"
#include "code.h"
#include "vm.h"
#include "value.h"

static void resetStack(aupVM *vm)
{
    vm->top = vm->stack;
    vm->frameCount = 0;
}

aupVM *aup_createVM(aupVM *from)
{
    aupVM *vm = malloc(sizeof(aupVM));
    memset(vm, '\0', sizeof(aupVM));

    if (from != NULL) {
        vm->gc = from->gc;
        vm->next = from->next;
        from->next = vm;
    }
    else {
        vm->gc = malloc(sizeof(aupGC));
        aup_initGC(vm->gc);
        vm->next = vm;
    }

    resetStack(vm);
    return vm;
}

void aup_closeVM(aupVM *vm)
{
    if (vm == NULL) return;

    if (vm->next == vm) {
        aup_freeGC(vm->gc);
        free(vm->gc);
    }

    free(vm);
}

static void runtimeError(aupVM *vm, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm->frameCount - 1; i >= 0; i--) {
        aupFrame *frame = &vm->frames[i];
        aupFun *function = frame->function;
        // -1 because the IP is sitting on the next instruction to be
        // executed.
        size_t offset = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[%d:%d] in ",
            function->chunk.lines[offset], function->chunk.columns[offset]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        }
        else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack(vm);
}

static bool call(aupVM *vm, aupFun *function, int argCount)
{
    if (argCount != function->arity) {
        runtimeError(vm, "Expected %d arguments but got %d.",
            function->arity, argCount);
        return false;
    }
    else if (vm->frameCount == AUP_MAX_FRAMES) {
        runtimeError(vm, "Stack overflow.");
        return false;
    }

    aupFrame *frame = &vm->frames[vm->frameCount++];
    frame->function = function;
    frame->ip = function->chunk.code;

    frame->stack = vm->top;
    return true;
}

static bool callValue(aupVM *vm, aupVal callee, int argCount)
{
    if (aup_isObj(callee)) {
        switch (aup_objType(callee)) {
            case AUP_OFUN:
                return call(vm, aup_asFun(callee), argCount);

            default:
                // Non-callable object type.                   
                break;
        }
    }

    runtimeError(vm, "Can only call functions and classes.");
    return false;
}

static int exec(aupVM *vm)
{
    register uint32_t *ip;
    register aupFrame *frame;
    register aupTab   *globals;
    register aupVal   left, right;

#define STORE_FRAME() \
	frame->ip = ip

#define LOAD_FRAME() \
	frame = &vm->frames[vm->frameCount - 1]; \
	ip = frame->ip

#define ERROR(fmt, ...) \
    STORE_FRAME(), \
    runtimeError(vm, fmt, ##__VA_ARGS__)

#define FETCH() aup_getOp(*ip++)
#define READ()  (ip[-1])

#define R(i)    (frame->stack[i])
#define K(i)    (frame->function->chunk.constants.values[i])

#define A       aup_getA(READ())
#define B       aup_getB(READ())
#define C       aup_getC(READ())

#define Op      aup_getOp(READ())
#define Axx     aup_getAxx(READ())
#define Bxx     aup_getBxx(READ())

#define RA      R(aup_getA(READ()))
#define RB      R(aup_getB(READ()))
#define RC      R(aup_getC(READ()))

#define KA      K(aup_getA(READ()))
#define KB      K(aup_getB(READ()))
#define KC      K(aup_getC(READ()))

#define sB      aup_getsB(READ())
#define sC      aup_getsC(READ())

#define RKB     (sB ? KB : RB)
#define RKC     (sC ? KC : RC)

#if defined(_MSC_VER)
// Switched goto
#define _CODE(x)        case AUP_OP_##x: goto _lbl_##x;
#define INTERPRET       switch (FETCH()) { AUP_OPCODES() default: goto _lbl_err; }
#define NEXT            INTERPRET
#define CODE(x)         _lbl_##x:
#define CODE_ERR()      _lbl_err:
#elif defined(__GNUC__) || defined(__clang__)
// Computed goto
#define _CODE(x)        &&_lbl_##x,
    static void *_lbls[AUP_OPCOUNT] = { AUP_OPCODES() };
#define INTERPRET       NEXT;
#define NEXT            goto *_lbls[FETCH()]
#define CODE(x)         _lbl_##x:
#define CODE_ERR()      _err:
#else
// Giant switch
#define INTERPRET       _loop: switch(FETCH())
#define NEXT            goto _loop
#define CODE(x)         case AUP_OP_##x:
#define CODE_ERR()      default:
#endif

    LOAD_FRAME();
    globals = &vm->gc->globals;

    INTERPRET
    {
        CODE(PRI) // @ %R
        {
            int start = A, count = B;
            for (int i = 0; i < count; i++) {
                aup_printValue(R(start + i));
                if (i < count - 1) printf("\t");
            }
            printf("\n");
            fflush(stdout);
            NEXT;
        }

        CODE(PSH) // %R
        {
            NEXT;
        }
        CODE(POP) // %R
        {
            NEXT;
        }

        CODE(NIL) // %R = nil
        {
            RA = aup_vNil;
            NEXT;
        }
        CODE(BOOL) // %R = %bool
        {
            RA = aup_vBool(sB);
            NEXT;
        }

        CODE(CALL) // %R %argc
        {
            int argc = B;

            STORE_FRAME();
            //vm->top = &R_A();

            if (!callValue(vm, *(vm->top = &RA), argc)) {
                return AUP_RUNTIME_ERROR;
            }

            LOAD_FRAME();
            NEXT;
        }
        CODE(RET) // ?isNil %RK
        {
            //closeUpvalues(vm, frame->stack);
            if (--vm->frameCount == 0) {
                //pop();
                return AUP_OK;
            }
            //vm->top = frame->stack;
            R(0) = A ? RKB : aup_vNil;
            LOAD_FRAME();
            NEXT;
        }

        CODE(JMP) // %offset
        {
            ip += Axx;
            NEXT;
        }
        CODE(JMPF) // %offset %RK
        {
            if (aup_isFalsey(RKC)) ip += Axx;
            NEXT;
        }
        CODE(JNE) // %offset %RK
        {
            int c = C;
            //if (!aup_isEqual(R(c - 1), R(c))) ip += Axx;
            if (memcmp(&R(c-1), &R(c), sizeof(aupVal)) != 0) ip += Axx;
            NEXT;
        }

        CODE(NOT) // %R %RK
        {
            RA = aup_vBool(aup_isFalsey(RKB));
            NEXT;
        } 
        CODE(LT) // %R = %RK < %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vBool(aup_asNum(left) < aup_asNum(right));
                    NEXT;
                default:
                    ERROR("Cannot perform < operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(LE) // %R = %RK <= %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vBool(aup_asNum(left) <= aup_asNum(right));
                    NEXT;
                default:
                    ERROR("Cannot perform <= operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(GT) // %R = %RK > %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vBool(aup_asNum(left) > aup_asNum(right));
                    NEXT;
                default:
                    ERROR("Cannot perform > operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(GE) // %R = %RK >= %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vBool(aup_asNum(left) >= aup_asNum(right));
                    NEXT;
                default:
                    ERROR("Cannot perform >= operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(EQ) // %R = %RK == %RK
        {
            RA = aup_vBool(aup_isEqual(RKB, RKC));
            NEXT;
        }

        CODE(NEG) // %R = -%RK
        {
            right = RKB;
            switch (aup_typeof(right)) {
                case AUP_TNUM:
                    RA = aup_vNum(-aup_asNum(right));
                    NEXT;
                case AUP_TBOOL:
                    RA = aup_vNum(-(char)aup_asBool(right));
                    NEXT;
                default:
                    ERROR("Cannot perform - operator, got <%s>.",
                        aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(ADD) // %R = %RK + %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vNum(aup_asNum(left) + aup_asNum(right));
                    NEXT;
                case AUP_TNUM_BOOL:
                    RA = aup_vNum(aup_asNum(left) + (char)aup_asBool(right));
                    NEXT;
                case AUP_TBOOL_NUM:               
                    RA = aup_vNum((char)aup_asBool(left) + aup_asNum(right));
                    NEXT;
                case AUP_TOBJ_NIL:
                case AUP_TOBJ_BOOL:
                case AUP_TOBJ_NUM:
                    // TODO
                case AUP_TNIL_OBJ:
                case AUP_TBOOL_OBJ:
                case AUP_TNUM_OBJ:
                    // TODO
                case AUP_TOBJ_OBJ:
                    // TODO
                default:
                    ERROR("Cannot perform + operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(SUB) // %R = %RK - %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vNum(aup_asNum(left) - aup_asNum(right));
                    NEXT;
                default:
                    ERROR("Cannot perform - operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(MUL) // %R = %RK * %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vNum(aup_asNum(left) * aup_asNum(right));
                    NEXT;
                default:
                    ERROR("Cannot perform * operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(DIV) // %R = %RK / %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vNum(aup_asNum(left) / aup_asNum(right));
                    NEXT;
                default:
                    ERROR("Cannot perform / operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(MOD) // %R = %RK % %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vNum(fmod(aup_asNum(left), aup_asNum(right)));
                    NEXT;
                default:
                    ERROR("Cannot perform % operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(POW) // %R = %RK ** %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vNum(pow(aup_asNum(left), aup_asNum(right)));
                    NEXT;
                default:
                    ERROR("Cannot perform ** operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }

        CODE(BNOT) // %R = ~%RK
        {
            right = RKB;
            switch (aup_typeof(right)) {
                case AUP_TNUM:
                    RA = aup_vNum(~aup_asI64(right));
                    NEXT;
                default:
                    ERROR("Cannot perform ~ operator, got <%s>.",
                        aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(BAND) // %R = %RK & %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vNum(aup_asI64(left) & aup_asI64(right));
                    NEXT;
                default:
                    ERROR("Cannot perform & operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(BOR) // %R = %RK | %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vNum(aup_asI64(left) | aup_asI64(right));
                    NEXT;
                default:
                    ERROR("Cannot perform | operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(BXOR) // %R = %RK ^ %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vNum(aup_asI64(left) ^ aup_asI64(right));
                    NEXT;
                default:
                    ERROR("Cannot perform ^ operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(SHL) // %R = %RK << %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vNum(aup_asI64(left) << aup_asI64(right));
                    NEXT;
                default:
                    ERROR("Cannot perform << operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }
        CODE(SHR) // %R = %RK >> %RK
        {
            left = RKB, right = RKC;
            switch (aup_cmb(aup_typeof(left), aup_typeof(right))) {
                case AUP_TNUM_NUM:
                    RA = aup_vNum(aup_asI64(left) >> aup_asI64(right));
                    NEXT;
                default:
                    ERROR("Cannot perform >> operator, got <%s> and <%s>.",
                        aup_typeName(left), aup_typeName(right));
                    return AUP_RUNTIME_ERROR;
            }
        }

        CODE(MOV) // %R = %R
        {
            RA = RB;
            NEXT;
        }
        CODE(LD) // %R = %RK
        {
            RA = RKB;
            NEXT;
        }

        CODE(GLD) // %R = G.%K
        {
            aupStr *name = aup_asStr(KB);
            aup_getKey(globals, name, &RA);
            NEXT;
        }
        CODE(GST) // G.%K = %RK (?nil)
        {
            aupStr *name = aup_asStr(KA);
            aup_setKey(globals, name, sC ? aup_vNil : RKB);
            NEXT;
        }

        CODE(GET)
        {
        }
        CODE(SET)
        {
        }

        CODE_ERR()
        {
            ERROR("Bad opcode, got %3d.", Op);
            return AUP_RUNTIME_ERROR;
        }
    }

    return AUP_OK;
}

int aup_interpret(aupVM *vm, aupSrc *source)
{
    aupFun *function = aup_compile(vm, source);
    if (function == NULL)
        return AUP_COMPILE_ERROR;

    //push(OBJ_VAL(function));
    *vm->top = aup_vObj(function);
    callValue(vm, aup_vObj(function), 0);

    return exec(vm);
}
