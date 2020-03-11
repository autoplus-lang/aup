#ifndef _AUP_CODE_H
#define _AUP_CODE_H
#pragma once

#include "util.h"
#include "value.h"

#define AUP_OPCODES() \
    _CODE(PRI)      \
    \
    _CODE(PSH)      \
    _CODE(POP)      \
    \
    _CODE(NIL)      \
    _CODE(BOOL)     \
    \
    _CODE(CALL)     \
    _CODE(RET)      \
    \
    _CODE(JMP)      \
    _CODE(JMPF)     \
    _CODE(JNE)      \
    \
    _CODE(NOT)      \
    _CODE(LT)       \
    _CODE(LE)       \
    _CODE(GT)       \
    _CODE(GE)       \
    _CODE(EQ)       \
    \
    _CODE(NEG)      \
    _CODE(ADD)      \
    _CODE(SUB)      \
    _CODE(MUL)      \
    _CODE(DIV)      \
    _CODE(MOD)      \
    _CODE(POW)      \
    \
    _CODE(BNOT)     \
    _CODE(BAND)     \
    _CODE(BOR)      \
    _CODE(BXOR)     \
    _CODE(SHL)      \
    _CODE(SHR)      \
    \
    _CODE(MOV)      \
    _CODE(LD)       \
    \
    _CODE(GLD)      \
    _CODE(GST)      \
    \
    _CODE(GET)      \
    _CODE(SET)

#define _CODE(x) AUP_OP_##x,
typedef enum { AUP_OPCODES() AUP_OPCOUNT } aupOp;
#undef _CODE

static const char *aup_opName(aupOp opcode) {
#define _CODE(x) #x,
    static const char *names[] = { AUP_OPCODES() };
    return names[opcode];
#undef _CODE
}

#define AUP_OpA(Op, A)              ((uint32_t)((Op) | (((A) & 0xFF) << 6)))
#define AUP_OpAB(Op, A, B)          ((uint32_t)((Op) | (((A) & 0xFF) << 6) | (((B)  & 0xFF)   << 14)))
#define AUP_OpAC(Op, A, C)          ((uint32_t)((Op) | (((A) & 0xFF) << 6) | (((C)  & 0xFF)   << 23)))
#define AUP_OpABC(Op, A, B, C)      ((uint32_t)((Op) | (((A) & 0xFF) << 6) | (((B)  & 0xFF)   << 14) | (((C)  & 0xFF)  << 23)))
#define AUP_OpABx(Op, A, Bx)        ((uint32_t)((Op) | (((A) & 0xFF) << 6) | (((Bx) & 0x1FF)  << 14) ))
#define AUP_OpACx(Op, A, Bx)        ((uint32_t)((Op) | (((A) & 0xFF) << 6) | (((Cx) & 0x1FF)  << 23) ))
#define AUP_OpABxCx(Op, A, Bx, Cx)  ((uint32_t)((Op) | (((A) & 0xFF) << 6) | (((Bx) & 0x1FF)  << 14) | (((Cx) & 0x1FF) << 23)))
#define AUP_OpAsB(Op, A, sB)        ((uint32_t)((Op) | (((A) & 0xFF) << 6) | (((sB) & 1)      << 22)))
#define AUP_OpAsC(Op, A, sC)        ((uint32_t)((Op) | (((A) & 0xFF) << 6) | (((sC) & 1)      << 31)))
#define AUP_OpAsBsC(Op, A, sB, sC)  ((uint32_t)((Op) | (((A) & 0xFF) << 6) | (((sB) & 1)      << 22) | (((sC) & 1)     << 31)))

#define AUP_OpAxx(Op, Axx)          ((uint32_t)((Op) | ((uint16_t)(Axx) << 6)))
#define AUP_OpAxxCx(Op, Axx, Cx)    ((uint32_t)((Op) | ((uint16_t)(Axx) << 6)                        | (((Cx) & 0x1FF) << 23)))
#define AUP_OpABxx(Op, A, Bxx)      ((uint32_t)((Op) | (((A) & 0xFF) << 6) | ((uint16_t)(Bxx) << 14)))

#define AUP_GetOp(i)                ((aupOp)   ( (i) &  0x3F       ))
#define AUP_GetA(i)                 ((uint8_t) ( (i) >> 6          ))
#define AUP_GetB(i)                 ((uint8_t) ( (i) >> 14         ))
#define AUP_GetC(i)                 ((uint8_t) ( (i) >> 23         ))
#define AUP_GetBx(i)                ((uint16_t)(((i) >> 14) & 0x1FF))
#define AUP_GetCx(i)                ((uint16_t)(((i) >> 23) & 0x1FF))
#define AUP_GetsB(i)                ((uint8_t) (((i) >> 22) & 1    ))
#define AUP_GetsC(i)                ((uint8_t) (((i) >> 31) & 1    ))
#define AUP_GetAxx(i)               ((int16_t) ( (i) >> 6          ))
#define AUP_GetBxx(i)               ((uint16_t)( (i) >> 14         ))

typedef struct {
    char   *buffer;
    char   *fname;
    size_t size;
} aupSrc;

aupSrc *aup_newSource(const char *fname);
void aup_freeSource(aupSrc *source);

typedef struct {
    int      count;
    int      space;
    uint32_t *code;
    uint16_t *lines;
    uint16_t *columns;
    aupSrc   *source;
    aupArr   constants;
} aupChunk;

void aup_initChunk(aupChunk *chunk, aupSrc *source);
void aup_freeChunk(aupChunk *chunk);
int  aup_emitChunk(aupChunk *chunk, uint32_t inst, int line, int column);
int  aup_addConstant(aupChunk *chunk, aupVal val);

typedef enum {
    // Characters
    AUP_TOK_LPAREN,             // (
    AUP_TOK_RPAREN,             // )
    AUP_TOK_LBRACKET,           // [
    AUP_TOK_RBRACKET,           // ]
    AUP_TOK_LBRACE,             // {
    AUP_TOK_RBRACE,             // }
    //
    AUP_TOK_COMMA,              // ,
    AUP_TOK_DOT,                // .
    AUP_TOK_COLON,              // :
    AUP_TOK_SEMICOLON,          // ;
    AUP_TOK_QMARK,              // ?
    //
    AUP_TOK_MINUS,              // -
    AUP_TOK_PLUS,               // +
    AUP_TOK_SLASH,              // /
    AUP_TOK_STAR,               // *
    AUP_TOK_PERCENT,            // %
    //
    AUP_TOK_AMPERSAND,          // &
    AUP_TOK_VBAR,               // |
    AUP_TOK_TILDE,              // ~
    AUP_TOK_CARET,              // ^
    AUP_TOK_LESS_LESS,          // <<
    AUP_TOK_GREATER_GREATER,    // >>
    //
    AUP_TOK_BANG,               // !
    AUP_TOK_BANG_EQUAL,         // !=
    AUP_TOK_EQUAL,              // =
    AUP_TOK_EQUAL_EQUAL,        // ==
    AUP_TOK_GREATER,            // >
    AUP_TOK_GREATER_EQUAL,      // >=
    AUP_TOK_LESS,               // <
    AUP_TOK_LESS_EQUAL,         // <=
    //
    AUP_TOK_AMPERSAND_EQUAL,    // &=
    AUP_TOK_VBAR_EQUAL,         // |=
    AUP_TOK_CARET_EQUAL,        // ^=
    //
    AUP_TOK_PLUS_EQUAL,         // +=
    AUP_TOK_MINUS_EQUAL,        // -=
    AUP_TOK_STAR_EQUAL,         // *=
    AUP_TOK_SLASH_EQUAL,        // /=
    AUP_TOK_PERCENT_EQUAL,      // %=
    //
    AUP_TOK_STAR_STAR,          // **
    AUP_TOK_QMARK_DOT,          // ?.
    AUP_TOK_ARROW,              // -> =>
    // Literals
    AUP_TOK_IDENTIFIER,         // [a-zA-Z_$][a-zA-Z_$0-9]+
    AUP_TOK_STRING,             // ".*?" '.*?'
    AUP_TOK_NUMBER,             // [0-9]+([.]?[0-9])*
    AUP_TOK_INTEGER,            // [0-9]+
    AUP_TOK_HEXADECIMAL,        // 0x[a-fA-F0-9]+
    // Keywords
    AUP_TOK_KW_AND,             // and
    AUP_TOK_KW_BREAK,           // break
    AUP_TOK_KW_CASE,            // case
    AUP_TOK_KW_CLASS,           // class
    AUP_TOK_KW_DO,              // do
    AUP_TOK_KW_ELSE,            // else
    AUP_TOK_KW_FALSE,           // false
    AUP_TOK_KW_FOR,             // for
    AUP_TOK_KW_FUNC,            // func
    AUP_TOK_KW_IF,              // if
    AUP_TOK_KW_MATCH,           // match
    AUP_TOK_KW_NIL,             // nil
    AUP_TOK_KW_NOT,             // not
    AUP_TOK_KW_OR,              // or
    AUP_TOK_KW_PUTS,            // puts
    AUP_TOK_KW_RETURN,          // return
    AUP_TOK_KW_SUPER,           // super
    AUP_TOK_KW_SWITCH,          // switch
    AUP_TOK_KW_THEN,            // then
    AUP_TOK_KW_THIS,            // this
    AUP_TOK_KW_TRUE,            // true
    AUP_TOK_KW_VAR,             // var
    AUP_TOK_KW_WHILE,           // while
    //
    AUP_TOK_ERROR,
    AUP_TOK_EOF
} aupTTok;

typedef struct {
    const char *start;
    const char *lineStart;
    aupTTok type;
    int length;
    int line;
    int column;
} aupTok;

void aup_initLexer(const char *source);
aupTok aup_scanToken();
aupTok aup_peekToken(int n);

aupFun *aup_compile(aupVM *vm, aupSrc *source);

int aup_dasmInst(aupChunk *chunk, int offset);
void aup_dasmChunk(aupChunk *chunk, const char *name);

#endif
