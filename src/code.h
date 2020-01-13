#ifndef _AUP_CODE_H
#define _AUP_CODE_H
#pragma once

#include "common.h"
#include "value.h"

#define OPCODES() \
/*        opcodes      args     stack       description */ \
    _CODE(PRINT)   	/* [n]      [-1, +0]    */ \
    _CODE(POP)     	/* []       [-1, +0]    */ \
    \
    _CODE(CALL)    	/* [n]      [-n, +1]    */ \
    _CODE(RET)     	/* []       [-1, +0]    */ \
    \
    _CODE(NIL)     	/* []       [-0, +1]    push nil to stack */ \
    _CODE(TRUE)    	/* []       [-0, +1]    push true to stack */ \
    _CODE(FALSE)   	/* []       [-0, +1]    push false to stack */ \
    _CODE(INT)   	/* [b]      [-0, +1]    */ \
    _CODE(INTL)   	/* [b, b]   [-0, +1]    */ \
    _CODE(CONST)   	/* [k]      [-0, +1]    push a constant from (k) to stack */ \
    \
    _CODE(NEG)     	/* []       [-1, +1]    */ \
    _CODE(NOT)     	/* []       [-1, +1]    */ \
    _CODE(BNOT)     /* []       [-1, +1]    */ \
    \
    _CODE(LT)      	/* []       [-1, +1]    */ \
    _CODE(LE)      	/* []       [-1, +1]    */ \
    _CODE(EQ)      	/* []       [-1, +1]    */ \
    \
    _CODE(ADD)     	/* []       [-2, +1]    */ \
    _CODE(SUB)     	/* []       [-2, +1]    */ \
    _CODE(MUL)     	/* []       [-2, +1]    */ \
    _CODE(DIV)     	/* []       [-2, +1]    */ \
    _CODE(MOD)     	/* []       [-2, +1]    */ \
    \
    _CODE(BAND)     /* []       [-2, +1]    */ \
    _CODE(BOR)     	/* []       [-2, +1]    */ \
    _CODE(BXOR)     /* []       [-2, +1]    */ \
    _CODE(SHL)     	/* []       [-2, +1]    */ \
    _CODE(SHR)     	/* []       [-2, +1]    */ \
    \
    _CODE(DEF)     	/* [k]      [-1, +0]    pop a value from stack and define as (k) in global */ \
    _CODE(GLD)     	/* [k]      [-0, +1]    push a from (k) in global to stack */ \
    _CODE(GST)     	/* [k]      [-0, +0]    set a value from stack as (k) in global */ \
    \
    _CODE(JMP)     	/* [s, s]   [-0, +0]    */ \
    _CODE(JMPF)    	/* [s, s]   [-0, +0]    */ \
    _CODE(JNE)      /* [s, s]   [-1, +0]    */ \
    _CODE(LOOP)     /* [s, s]   [-0, +0]    */ \
    \
    _CODE(LD)      	/* [s]      [-0, +1]    */ \
    _CODE(ST)      	/* [s]      [-0, +0]    */ \
    _CODE(MAP)      /* []       [-0, +1]    */ \
    _CODE(GET)      /* [k]      [-1, +1]    */ \
    _CODE(SET)      /* [k]      [-2, +1]    */ \
    _CODE(GETI)     /* []       [-2, +1]    */ \
    _CODE(SETI)     /* []       [-3, +1]    */ \
    \
    _CODE(CLOSURE)  /* [k, ...] [-0, +0]    */ \
    _CODE(CLOSE)    /* []       [-1, +0]    */ \
    _CODE(ULD)      /* [u]      [-0, +1]    */ \
    _CODE(UST)      /* [u]      [-0, +0]    */

#define _CODE(x) AUP_OP_##x,
typedef enum { OPCODES() AUP_OPCOUNT } aupOp;
#undef _CODE

#define AUP_CODEPAGE    256

typedef struct {
    char *buffer;
    char *fname;
    size_t size;
} aupSrc;

aupSrc *aup_newSource(const char *file);
void aup_freeSource(aupSrc *source);

typedef struct {
    int count;
    int capacity;
    uint8_t *code;
    uint16_t *lines;
    uint16_t *columns;
    aupSrc *source;
    aupArr constants;
} aupChunk;

void aup_initChunk(aupChunk *chunk, aupSrc *source);
void aup_freeChunk(aupChunk *chunk);
void aup_emitChunk(aupChunk *chunk, uint8_t byte, int line, int column);

void aup_dasmChunk(aupChunk *chunk, const char *name);
int aup_dasmInstruction(aupChunk *chunk, int offset);

static const char *aup_op2Str(aupOp opcode) {
#define _CODE(x) #x,
    static const char *tab[] = { OPCODES() };
#undef _CODE
    return tab[opcode];
}

typedef enum {
    // Single-character tokens.                         
    AUP_TOK_LPAREN,
    AUP_TOK_RPAREN,
    AUP_TOK_LBRACKET,
    AUP_TOK_RBRACKET,
    AUP_TOK_LBRACE,
    AUP_TOK_RBRACE,

    AUP_TOK_ARROW,
    AUP_TOK_COMMA,
    AUP_TOK_DOT,
    AUP_TOK_SEMICOLON,

    AUP_TOK_COLON,
    AUP_TOK_QMARK,

    AUP_TOK_MINUS, 
    AUP_TOK_PLUS,
    AUP_TOK_SLASH,
    AUP_TOK_STAR,
    AUP_TOK_PERCENT,

    AUP_TOK_AMPERSAND,
    AUP_TOK_VBAR,
    AUP_TOK_TILDE,
    AUP_TOK_CARET,
    AUP_TOK_LESS_LESS,
    AUP_TOK_GREATER_GREATER,

    // One or two character tokens.                     
    AUP_TOK_BANG,
    AUP_TOK_BANG_EQUAL,
    AUP_TOK_EQUAL,
    AUP_TOK_EQUAL_EQUAL,
    AUP_TOK_GREATER,
    AUP_TOK_GREATER_EQUAL,
    AUP_TOK_LESS,
    AUP_TOK_LESS_EQUAL,

    AUP_TOK_PLUS_EQUAL,
    AUP_TOK_MINUS_EQUAL,
    AUP_TOK_STAR_EQUAL,
    AUP_TOK_SLASH_EQUAL,
    AUP_TOK_PERCENT_EQUAL,

    // Literals.                                        
    AUP_TOK_IDENTIFIER,
    AUP_TOK_STRING,
    AUP_TOK_NUMBER,
    AUP_TOK_INTEGER,
    AUP_TOK_HEXADECIMAL,

    // Keywords.                                        
    AUP_TOK_AND,
    AUP_TOK_CLASS,
    AUP_TOK_ELSE,
    AUP_TOK_FALSE,
    AUP_TOK_FOR,
    AUP_TOK_FUNC,
    AUP_TOK_IF,
    AUP_TOK_LOOP,
    AUP_TOK_MATCH,
    AUP_TOK_NIL,
    AUP_TOK_NOT,
    AUP_TOK_OR,
    AUP_TOK_PRINT,
    AUP_TOK_RETURN,
    AUP_TOK_SUPER,
    AUP_TOK_THIS,
    AUP_TOK_TRUE,
    AUP_TOK_VAR,
    AUP_TOK_WHILE,

    AUP_TOK_ERROR,
    AUP_TOK_EOF,

    AUP_TOKENCOUNT
} aupTokType;

typedef struct {
    const char *start;
    const char *lineStart;
    int lineLength;
    aupTokType type;
    int length;
    int line;
    int column;
} aupTok;

typedef struct {
    const char *start;
    const char *current;
    const char *lineStart;
    int lineLength;
    int line;
    int position;
} aupLexer;

typedef struct _aupCompiler aupCompiler;

void aup_initLexer(aupLexer *lexer, const char *source);
aupTok aup_scanToken(aupLexer *lexer);

aupFun *aup_compile(aupVM *vm, aupSrc *source);
void aup_markCompilerRoots(aupVM *vm);

#endif
