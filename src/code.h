#pragma once

#include "common.h"
#include "value.h"

#define OPCODES() \
/*        opcodes      args     stack       description */ \
    _CODE(PRINT)   	/* []       [-1, +0]    pop a value from stack */ \
    _CODE(POP)     	/* []       [-1, +0]    pop a value from stack and print it */ \
    _CODE(CALL)    	/* [n]      [-n, +1]    */ \
    _CODE(RET)     	/* []       [-1, +0]    */ \
    _CODE(NIL)     	/* []       [-0, +1]    push nil to stack */ \
    _CODE(TRUE)    	/* []       [-0, +1]    push true to stack */ \
    _CODE(FALSE)   	/* []       [-0, +1]    push false to stack */ \
    _CODE(CONST)   	/* [k]      [-0, +1]    push a constant from (k) to stack */ \
    _CODE(NEG)     	/* []       [-1, +1]    */ \
    _CODE(NOT)     	/* []       [-1, +1]    */ \
    _CODE(LT)      	/* []       [-1, +1]    */ \
    _CODE(LE)      	/* []       [-1, +1]    */ \
    _CODE(EQ)      	/* []       [-1, +1]    */ \
    _CODE(ADD)     	/* []       [-2, +1]    */ \
    _CODE(SUB)     	/* []       [-2, +1]    */ \
    _CODE(MUL)     	/* []       [-2, +1]    */ \
    _CODE(DIV)     	/* []       [-2, +1]    */ \
    _CODE(MOD)     	/* []       [-2, +1]    */ \
    _CODE(DEF)     	/* [k]      [-1, +0]    pop a value from stack and define as (k) in global */ \
    _CODE(GLD)     	/* [k]      [-0, +1]    push a from (k) in global to stack */ \
    _CODE(GST)     	/* [k]      [-0, +0]    set a value from stack as (k) in global */ \
    _CODE(JMP)     	/* [s, s]   [-0, +0]    */ \
    _CODE(JMPF)    	/* [s, s]   [-1, +0]    */ \
    _CODE(LD)      	/* [s]      [-0, +1]    */ \
    _CODE(ST)      	/* [s]      [-0, +0]    */ \
    _CODE(MAP)      /* []       [-0, +1]    */ \
    _CODE(GET)      \
    _CODE(SET)      \
    _CODE(GETI)     \
    _CODE(SETI)

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

static const char *aup_op2Str(aupOp opcode) {
#define _CODE(x) #x,
    static const char *tab[] = { OPCODES() };
#undef _CODE
    return tab[opcode];
}

typedef enum {
    // Single-character tokens.                         
    AUP_TOK_LPAREN, AUP_TOK_RPAREN,
    AUP_TOK_LBRACKET, AUP_TOK_RBRACKET,
    AUP_TOK_LBRACE, AUP_TOK_RBRACE,
    AUP_TOK_COMMA, AUP_TOK_DOT, AUP_TOK_MINUS, AUP_TOK_PLUS,
    AUP_TOK_SEMICOLON, AUP_TOK_SLASH, AUP_TOK_STAR,

    // One or two character tokens.                     
    AUP_TOK_BANG, AUP_TOK_BANG_EQUAL,
    AUP_TOK_EQUAL, AUP_TOK_EQUAL_EQUAL,
    AUP_TOK_GREATER, AUP_TOK_GREATER_EQUAL,
    AUP_TOK_LESS, AUP_TOK_LESS_EQUAL,

    // Literals.                                        
    AUP_TOK_IDENTIFIER, AUP_TOK_STRING, AUP_TOK_NUMBER,

    // Keywords.                                        
    AUP_TOK_AND, AUP_TOK_CLASS, AUP_TOK_ELSE, AUP_TOK_FALSE,
    AUP_TOK_FOR, AUP_TOK_FUN, AUP_TOK_IF, AUP_TOK_NIL, AUP_TOK_OR,
    AUP_TOK_PRINT, AUP_TOK_RETURN, AUP_TOK_SUPER, AUP_TOK_THIS,
    AUP_TOK_TRUE, AUP_TOK_VAR, AUP_TOK_WHILE,

    AUP_TOK_ERROR,
    AUP_TOK_EOF
} aupTokType;

typedef struct {
    const char *start;
    const char *currentLine;
    aupTokType type;
    int length;
    int line;
    int column;
} aupTok;

typedef struct {
    const char *start;
    const char *current;
    const char *currentLine;
    int line;
    int position;
} aupLexer;

void aup_initLexer(aupLexer *lexer, const char *source);
aupTok aup_scanToken(aupLexer *lexer);

aupFun *aup_compile(aupVM *vm, aupSrc *source);
