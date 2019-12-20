#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"

#define REG int

typedef struct {
	aupTk current;
	aupTk previous;
	bool hadError;
	bool panicMode;
} aupPs;

typedef enum {
	PREC_NONE,
	PREC_ASSIGNMENT,  // =        
	PREC_OR,          // or       
	PREC_AND,         // and      
	PREC_EQUALITY,    // == !=    
	PREC_COMPARISON,  // < > <= >=
	PREC_TERM,        // + -      
	PREC_FACTOR,      // * /      
	PREC_UNARY,       // ! -      
	PREC_CALL,        // . ()     
	PREC_PRIMARY
} Precedence;

typedef void (* ParseFn)();

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

static aupPs parser;
static aupCh *compilingChunk;

static aupCh *currentChunk()
{
	return compilingChunk;
}

static void errorAt(aupTk *token, const char *msgf, ...)
{
	if (parser.panicMode) return;
	parser.panicMode = true;

	fprintf(stderr, "[%d:%d] Error", token->line, token->column);

	if (token->type == TOKEN_EOF) {
		fprintf(stderr, " at end");
	}
	else if (token->type == TOKEN_ERROR) {
		// Nothing.                                                
	}
	else {
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}

	fprintf(stderr, ": ");
	va_list ap;
	va_start(ap, msgf);
	vfprintf(stderr, msgf, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	parser.hadError = true;
}

#define error(msgf, ...) \
	errorAt(&parser.previous, msgf, ##__VA_ARGS__)
#define errorAtCurrent(msgf, ...) \
	errorAt(&parser.current, msgf, ##__VA_ARGS__)

static void advance()
{
	parser.previous = parser.current;

	for (;;) {
		parser.current = aupLx_scan();
		if (parser.current.type != TOKEN_ERROR) break;

		errorAtCurrent(parser.current.start);
	}
}

#define consume(token_type, msgf, ...) \
	do { \
		if (parser.current.type == token_type) { \
			advance(); \
			break; \
		} \
		errorAt(&parser.current, msgf, ##__VA_ARGS__); \
	} while(0)

static void emit(uint32_t instruction)
{
	aupCh_write(currentChunk(), instruction,
		parser.previous.line, parser.previous.column);
}

#define _OPCODE(x)					AUP_OP_##x

#define EMIT_Op(op)					emit(_OPCODE(op))
#define EMIT_OpA(op, A)				emit(AUP_SET_OpA(_OPCODE(op), A))
#define EMIT_OpAx(op, Ax)			emit(AUP_SET_OpAx(_OPCODE(op), Ax))
#define EMIT_OpAxCx(op, Ax, Cx)		emit(AUP_SET_OpAxCx(_OPCODE(op), Ax, Cx))

#define EMIT_OpAB(op, A, B)			emit(AUP_SET_OpAB(_OPCODE(op), A, B))
#define EMIT_OpABC(op, A, B, C)		emit(AUP_SET_OpABC(_OPCODE(op), A, B, C))

#define EMIT_OpABx(op, A, Bx)		emit(AUP_SET_OpABx(_OPCODE(op), A, Bx))
#define EMIT_OpABxCx(op, A, Bx, Cx)	emit(AUP_SET_OpABxCx(_OPCODE(op), A, Bx, Cx))

#define EMIT_OpAsB(op, A, sB)		emit(AUP_SET_OpAsB(_OPCODE(op), A, sB))
#define EMIT_OpAsC(op, A, sC)		emit(AUP_SET_OpAsC(_OPCODE(op), A, sC))
#define EMIT_OpAsBsC(op, A, sB, sC)	emit(AUP_SET_OpAsBsC(_OPCODE(op), A, sB, sC))

static uint8_t makeConstant(aupV value)
{
	int constant = aupCh_addK(currentChunk(), value);
	if (constant > AUP_MAX_CONSTS) {
		error("Too many constants in one chunk.");
		return 0;
	}

	return (uint8_t)constant;
}

static void emitConstant(aupV value)
{
	//emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler()
{
	//emitReturn();
}

static void expression();
static ParseRule *getRule(aupTkt type);
static void parsePrecedence(Precedence precedence);

static void binary()
{
	// Remember the operator.                                
	aupTkt operatorType = parser.previous.type;

	// Compile the right operand.                            
	ParseRule* rule = getRule(operatorType);
	parsePrecedence((Precedence)(rule->precedence + 1));

	// Emit the operator instruction.                        
	switch (operatorType) {
		case TOKEN_PLUS:          //emitByte(OP_ADD); break;
		case TOKEN_MINUS:        // emitByte(OP_SUBTRACT); break;
		case TOKEN_STAR:         // emitByte(OP_MULTIPLY); break;
		case TOKEN_SLASH:        // emitByte(OP_DIVIDE); break;
		default:
			return; // Unreachable.                              
	}
}

static void grouping()
{
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number()
{
}

static void unary()
{
	aupTkt operatorType = parser.previous.type;

	// Compile the operand.                        
	parsePrecedence(PREC_UNARY);

	// Emit the operator instruction.              
	switch (operatorType) {
		case TOKEN_MINUS: //emitByte(OP_NEGATE); break;
		default:
			return; // Unreachable.                    
	}
}

static ParseRule rules[] = {
	{ grouping, NULL,    PREC_NONE },       // TOKEN_LEFT_PAREN      
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_PAREN     
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_LEFT_BRACE
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACE     
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_COMMA           
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_DOT             
	{ unary,    binary,  PREC_TERM },       // TOKEN_MINUS           
	{ NULL,     binary,  PREC_TERM },       // TOKEN_PLUS            
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_SEMICOLON       
	{ NULL,     binary,  PREC_FACTOR },     // TOKEN_SLASH           
	{ NULL,     binary,  PREC_FACTOR },     // TOKEN_STAR            
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_BANG            
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_BANG_EQUAL      
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL           
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL_EQUAL     
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_GREATER         
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_GREATER_EQUAL   
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_LESS            
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_LESS_EQUAL      
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_IDENTIFIER      
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_STRING          
	{ number,   NULL,    PREC_NONE },       // TOKEN_NUMBER          
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_AND             
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_CLASS           
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE            
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_FALSE           
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_FOR             
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_FUN             
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_IF              
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_NIL             
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_OR              
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_PRINT           
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_RETURN          
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_SUPER           
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_THIS            
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_TRUE            
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_VAR             
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_WHILE           
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_ERROR           
	{ NULL,     NULL,    PREC_NONE },       // TOKEN_EOF             
};

static void parsePrecedence(Precedence precedence)
{
	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (prefixRule == NULL) {
		error("Expect expression.");
		return;
	}

	prefixRule();

	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule();
	}
}

static ParseRule *getRule(aupTkt type)
{
	return &rules[type];
}

static void expression()
{
	parsePrecedence(PREC_ASSIGNMENT);
}

bool aup_compile(const char *source, aupCh *chunk)
{
	aupLx_init(source);
	compilingChunk = chunk;

	parser.hadError = false;
	parser.panicMode = false;

	advance();
	expression();
	consume(TOKEN_EOF, "Expect end of expression.");
	endCompiler();

	return !parser.hadError;
}