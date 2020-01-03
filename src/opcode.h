#ifndef _AUP_OPCODE_H
#define _AUP_OPCODE_H
#pragma once

#include <stdint.h>

/*
        0 1 2 3 4 5 6 7 8 9 ...                                   ... 31
        [_6_op____] [_8_A_________] [_9_Bx__________] [_9_Cx__________]
                                    [_8_B_________] [ [_8_C_________] [
                    [_16_Ax_______________________] sB]               sC]

                            32-bits instruction
*/

typedef union {
    struct {
        unsigned op : 6;
        unsigned  A : 8;
        unsigned Bx : 9;
        unsigned Cx : 9;
    };
    struct {
        unsigned : 14;
        unsigned  B : 8;
        unsigned sB : 1;
        unsigned  C : 8;
        unsigned sC : 1;
    };
    struct {
        unsigned : 6;
        signed   Ax : 16;
    };
    unsigned raw : 32;
} aupI;

#define AUP_SET_OpA(op, A)              ((uint32_t)( (op) | ((A) << 6) ))
#define AUP_SET_OpAx(op, Ax)            ((uint32_t)( (op) | ((uint16_t)(Ax) << 6) ))
#define AUP_SET_OpAxCx(op, Ax, Cx)      ((uint32_t)( (op) | ((uint16_t)(Ax) << 6) | (((Cx) & 0x1FF) << 23) ))
#define AUP_SET_OpAB(op, A, B)          ((uint32_t)( (op) | (((A) & 0xFF) << 6) | (((B) & 0xFF) << 14) ))
#define AUP_SET_OpABC(op, A, B, C)      ((uint32_t)( (op) | (((A) & 0xFF) << 6) | (((B) & 0xFF) << 14) | (((C) & 255) << 23)  ))

#define AUP_SET_OpABx(op, A, Bx)        ((uint32_t)( (op) | (((A) & 0xFF) << 6) | (((Bx) & 0x1FF) << 14) ))
#define AUP_SET_OpABxCx(op, A, Bx, Cx)  ((uint32_t)( (op) | (((A) & 0xFF) << 6) | (((Bx) & 0x1FF) << 14) | (((Cx) & 511) << 23) ))

#define AUP_SET_OpAsB(op, A, sB)        ((uint32_t)( (op) | (((A) & 0xFF) << 6) | (((sB) & 1) << 22) ))
#define AUP_SET_OpAsC(op, A, sC)        ((uint32_t)( (op) | (((A) & 0xFF) << 6) | (((sC) & 1) << 31) ))
#define AUP_SET_OpAsBsC(op, A, sB, sC)  ((uint32_t)( (op) | (((A) & 0xFF) << 6) | (((sB) & 1) << 22) | (((sC) & 1) << 31) ))

#define AUP_GET_Op(i)   ((aupOp)((i) & 0x3F))

#define AUP_GET_A(i)    ((uint8_t)((i) >> 6))
#define AUP_GET_Ax(i)   ((int16_t)((i) >> 6))

#define AUP_GET_B(i)    ((uint8_t)((i) >> 14))
#define AUP_GET_Bx(i)   ((uint16_t)(((i) >> 14) & 0x1FF))
#define AUP_GET_sB(i)   ((uint8_t)(((i) >> 22) & 1))

#define AUP_GET_C(i)    ((uint8_t)((i) >> 23))
#define AUP_GET_Cx(i)   ((uint16_t)(((i) >> 23) & 0x1FF))
#define AUP_GET_sC(i)   ((uint8_t)(((i) >> 31) & 1))

#define AUP_OPCODES()   \
    _CODE(CALL)         \
    _CODE(RET)          \
    _CODE(PUTS)         \
    _CODE(NIL)          \
    _CODE(BOOL)         \
    _CODE(CONST)        \
    _CODE(NOT)          \
    _CODE(NEG)          \
    _CODE(LT)           \
    _CODE(LE)           \
    _CODE(EQ)           \
    _CODE(ADD)          \
    _CODE(SUB)          \
    _CODE(MUL)          \
    _CODE(DIV)          \
    _CODE(MOD)          \
    _CODE(MOV)          \
    _CODE(GLD)          \
    _CODE(GST)          \
    _CODE(LD)           \
    _CODE(ST)           \
    _CODE(ULD)          \
    _CODE(UST)          \
    _CODE(JMP)          \
    _CODE(JMPF)         \
    _CODE(CLOSURE)      \
    _CODE(CLOSE)

#define _CODE(x) AUP_OP_##x,
typedef enum { AUP_OPCODES() } aupOp;
#undef _CODE

static inline const char *aup_opName(aupOp opcode) {
#define _CODE(x) /* [AUP_OP_##x] = */ #x,
    static const char *names[] = { AUP_OPCODES() };
    return names[opcode];
#undef _CODE
}

#endif
