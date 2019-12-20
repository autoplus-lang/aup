#ifndef _AUP_COMPILER_H
#define _AUP_COMPILER_H
#pragma once

#include "chunk.h"

typedef enum {
	// Single-character tokens.                         
	TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
	TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
	TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
	TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,

	// One or two character tokens.                     
	TOKEN_BANG, TOKEN_BANG_EQUAL,
	TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
	TOKEN_GREATER, TOKEN_GREATER_EQUAL,
	TOKEN_LESS, TOKEN_LESS_EQUAL,

	// Literals.                                        
	TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

	// Keywords.                                        
	TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
	TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
	TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
	TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,

	TOKEN_ERROR,
	TOKEN_EOF
} aupTkt;

typedef struct {
	int type;
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

bool aup_compile(const char *source, aupCh *chunk);

#endif
