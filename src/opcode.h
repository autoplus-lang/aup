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

typedef enum {
    AUP_OP_NOP = 0,

    AUP_OP_CALL,
    AUP_OP_RET,

    AUP_OP_PUSH,
    AUP_OP_POP,
    AUP_OP_PUT,

    AUP_OP_NIL,
    AUP_OP_BOOL,

    AUP_OP_NOT,
    AUP_OP_NEG,

    AUP_OP_LT,
    AUP_OP_LE,
    AUP_OP_EQ,

    AUP_OP_ADD,
    AUP_OP_SUB,
    AUP_OP_MUL,
    AUP_OP_DIV,
    AUP_OP_MOD,

    AUP_OP_MOV,

    AUP_OP_GLD,
    AUP_OP_GST,

    AUP_OP_LD,
    AUP_OP_ST,

    AUP_OP_CLU,
    AUP_OP_CLO,
    AUP_OP_ULD,
    AUP_OP_UST,

    AUP_OP_JMP,
    AUP_OP_JMPF,
} aupOp;

#endif
