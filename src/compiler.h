#ifndef _AUP_COMPILER_H
#define _AUP_COMPILER_H
#pragma once

#include "util.h"
#include "chunk.h"

typedef enum {
	// Single-character tokens.                         
	TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
	TOKEN_LBRACE,
    TOKEN_RBRACE,

	TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_COLON,
    TOKEN_SEMICOLON,

    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_SLASH,
    TOKEN_STAR,
	TOKEN_PERCENT,

	// One or two character tokens.                     
	TOKEN_BANG,
    TOKEN_BANG_EQ,
	TOKEN_EQUAL,
    TOKEN_EQUAL_EQ,
	TOKEN_GREATER,
    TOKEN_GREATER_EQ,
	TOKEN_LESS,
    TOKEN_LESS_EQ,

    TOKEN_PLUS_EQ,
    TOKEN_MINUS_EQ,
    TOKEN_STAR_EQ,
    TOKEN_SLASH_EQ,
    TOKEN_PERCENT_EQ,

	// Literals.                                        
	TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
	TOKEN_INTEGER,
    TOKEN_BINARY,
    TOKEN_OCTAL,
    TOKEN_HEXADECIMAL,

	// Keywords.                                        
	TOKEN_AND,
    TOKEN_CLASS,
    TOKEN_DO,
    TOKEN_ELSE,
    TOKEN_END,
    TOKEN_FALSE,
	TOKEN_FOR,
    TOKEN_FUNC,
    TOKEN_IF,
    TOKEN_NIL,
    TOKEN_OR,
	TOKEN_PUTS,
    TOKEN_RETURN,
    TOKEN_SUPER,
    TOKEN_THEN,
    TOKEN_THIS,
	TOKEN_TRUE,
    TOKEN_VAR,
    TOKEN_WHILE,

	TOKEN_ERROR,
	TOKEN_EOF
} aupTkt;

typedef struct {
	aupTkt type;
	const char* start;
	int length;
	int line;
	int column;
} aupTk;

#define AUP_MAX_ARGS	32
#define AUP_MAX_CONSTS	256
#define AUP_MAX_LOCALS	224

void aupLx_init(const char *source);
aupTk aupLx_scan();

aupOf *aup_compile(AUP_VM, const char *source);

#endif
