#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "object.h"
#include "vm.h"

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

#define REG		int
typedef void (* ParseFn)(REG dest);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

static aupPs parser;
static aupCh *compilingChunk;
static aupVM *runningVM;

static aupCh *currentChunk()
{
	return compilingChunk;
}

static aupVM *currentVM()
{
	return runningVM;
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

static bool check(aupTkt type)
{
	return parser.current.type == type;
}

static bool match(aupTkt type)
{
	if (!check(type)) return false;
	advance();
	return true;
}

static void emit(uint32_t instruction)
{
	aupCh_write(currentChunk(), instruction,
		parser.previous.line, parser.previous.column);
}

static  REG		currentReg;

#define PUSH()	currentReg++
#define POP()	--currentReg
#define RESET()	currentReg = 0

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

	if (!parser.hadError) {
		aupCh_dasm(currentChunk(), "code");
	}
}

static REG expression(REG dest);
static void statement();
static void declaration();
static ParseRule *getRule(aupTkt type);
static REG parsePrecedence(Precedence precedence, REG dest);

static uint8_t identifierConstant(aupTk *name)
{
	return makeConstant(AUP_OBJ(aupOs_copy(currentVM(), name->start, name->length)));
}

static uint8_t parseVariable(const char *errorMessage)
{
	consume(TOKEN_IDENTIFIER, errorMessage);
	return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global)
{
	//emitBytes(OP_DEFINE_GLOBAL, global);
}

static void binary(REG dest)
{
	// Remember the operator.                                
	aupTkt operatorType = parser.previous.type;

	// Compile the right operand.                            
	ParseRule* rule = getRule(operatorType);
	parsePrecedence((Precedence)(rule->precedence + 1), dest);

	// Emit the operator instruction.                        
	switch (operatorType) {
		case TOKEN_BANG_EQUAL:    //emitBytes(OP_EQUAL, OP_NOT); break;
		case TOKEN_EQUAL_EQUAL:   //emitByte(OP_EQUAL); break;
		case TOKEN_GREATER:       //emitByte(OP_GREATER); break;
		case TOKEN_GREATER_EQUAL: //emitBytes(OP_LESS, OP_NOT); break;
		case TOKEN_LESS:          //emitByte(OP_LESS); break;
		case TOKEN_LESS_EQUAL:    //emitBytes(OP_GREATER, OP_NOT); break;

		case TOKEN_PLUS:          //emitByte(OP_ADD); break;
		case TOKEN_MINUS:        // emitByte(OP_SUBTRACT); break;
		case TOKEN_STAR:         // emitByte(OP_MULTIPLY); break;
		case TOKEN_SLASH:        // emitByte(OP_DIVIDE); break;
		default:
			return; // Unreachable.                              
	}
}

static void literal(REG dest)
{
	switch (parser.previous.type) {
		case TOKEN_FALSE: //emitByte(OP_FALSE); break;
		case TOKEN_NIL: //emitByte(OP_NIL); break;
		case TOKEN_TRUE: //emitByte(OP_TRUE); break;
		default:
			return; // Unreachable.                   
	}
}

static void grouping(REG dest)
{
	expression(dest);
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(REG dest)
{
	//double num = strtod(parser.previous.start, NULL);
}

static void string(REG dest)
{
	//int len = parser.previous.length - 2;
	//const char *src = parser.previous.start + 1;
}

static void unary(REG dest)
{
	aupTkt operatorType = parser.previous.type;

	// Compile the operand.                        
	parsePrecedence(PREC_UNARY, dest);

	// Emit the operator instruction.              
	switch (operatorType) {
		case TOKEN_BANG: //emitByte(OP_NOT); break;
		case TOKEN_MINUS: //emitByte(OP_NEGATE); break;
		default:
			return; // Unreachable.                    
	}
}

static ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]      = { grouping, NULL,    PREC_NONE },
    [TOKEN_RIGHT_PAREN]     = { NULL,     NULL,    PREC_NONE },
    [TOKEN_LEFT_BRACE]      = { NULL,     NULL,    PREC_NONE },
    [TOKEN_RIGHT_BRACE]     = { NULL,     NULL,    PREC_NONE },

    [TOKEN_COMMA]           = { NULL,     NULL,    PREC_NONE },
    [TOKEN_DOT]             = { NULL,     NULL,    PREC_NONE },
    [TOKEN_MINUS]           = { unary,    binary,  PREC_TERM },
    [TOKEN_PLUS]            = { NULL,     binary,  PREC_TERM },
    [TOKEN_SEMICOLON]       = { NULL,     NULL,    PREC_NONE },
    [TOKEN_SLASH]           = { NULL,     binary,  PREC_FACTOR },
    [TOKEN_STAR]            = { NULL,     binary,  PREC_FACTOR },

    [TOKEN_BANG]            = { unary,    NULL,    PREC_NONE },
    [TOKEN_BANG_EQUAL]      = { NULL,     binary,  PREC_EQUALITY },
    [TOKEN_EQUAL]           = { NULL,     NULL,    PREC_NONE },
    [TOKEN_EQUAL_EQUAL]     = { NULL,     binary,  PREC_EQUALITY },
    [TOKEN_GREATER]         = { NULL,     binary,  PREC_COMPARISON },
    [TOKEN_GREATER_EQUAL]   = { NULL,     binary,  PREC_COMPARISON },
    [TOKEN_LESS]            = { NULL,     binary,  PREC_COMPARISON },
    [TOKEN_LESS_EQUAL]      = { NULL,     binary,  PREC_COMPARISON },

    [TOKEN_IDENTIFIER]      = { NULL,     NULL,    PREC_NONE },
    [TOKEN_STRING]          = { string,   NULL,    PREC_NONE },
    [TOKEN_NUMBER]          = { number,   NULL,    PREC_NONE },

    [TOKEN_AND]             = { NULL,     NULL,    PREC_NONE },
    [TOKEN_CLASS]           = { NULL,     NULL,    PREC_NONE },
    [TOKEN_ELSE]            = { NULL,     NULL,    PREC_NONE },
    [TOKEN_FALSE]           = { literal,  NULL,    PREC_NONE },
    [TOKEN_FOR]             = { NULL,     NULL,    PREC_NONE },
    [TOKEN_FUN]             = { NULL,     NULL,    PREC_NONE },
    [TOKEN_IF]              = { NULL,     NULL,    PREC_NONE },
    [TOKEN_NIL]             = { literal,  NULL,    PREC_NONE },
    [TOKEN_OR]              = { NULL,     NULL,    PREC_NONE },
    [TOKEN_PUTS]            = { NULL,     NULL,    PREC_NONE },
    [TOKEN_RETURN]          = { NULL,     NULL,    PREC_NONE },
    [TOKEN_SUPER]           = { NULL,     NULL,    PREC_NONE },
    [TOKEN_THIS]            = { NULL,     NULL,    PREC_NONE },
    [TOKEN_TRUE]            = { literal,  NULL,    PREC_NONE },
    [TOKEN_VAR]             = { NULL,     NULL,    PREC_NONE },
    [TOKEN_WHILE]           = { NULL,     NULL,    PREC_NONE },

    [TOKEN_ERROR]           = { NULL,     NULL,    PREC_NONE },
    [TOKEN_EOF]             = { NULL,     NULL,    PREC_NONE },
};

static REG parsePrecedence(Precedence precedence, REG dest)
{
	if (dest <= -1) dest = PUSH();

	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (prefixRule == NULL) {
		error("Expect expression.");
		return -1;
	}

	prefixRule(dest);

	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(dest);
	}

	return dest;
}

static ParseRule *getRule(aupTkt type)
{
	return &rules[type];
}

static REG expression(REG dest)
{
	return parsePrecedence(PREC_ASSIGNMENT, dest);
}

static void varDeclaration()
{
	uint8_t global = parseVariable("Expect variable name.");

	if (match(TOKEN_EQUAL)) {
		expression(-1);
	}
	else {
		//emitByte(OP_NIL);
	}
	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

	defineVariable(global);
}

static void expressionStatement()
{
	expression(-1);
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	//emitByte(OP_POP);
}

static void putsStatement()
{
	expression(-1);
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	//emitByte(OP_PRINT);
}

static void synchronize()
{
	parser.panicMode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON) return;

		switch (parser.current.type) {
			case TOKEN_CLASS:
			case TOKEN_FUN:
			case TOKEN_VAR:
			case TOKEN_FOR:
			case TOKEN_IF:
			case TOKEN_WHILE:
			case TOKEN_PUTS:
			case TOKEN_RETURN:
				return;

			default:
				// Do nothing.                                  
				;
		}

		advance();
	}
}

static void declaration()
{
	if (match(TOKEN_VAR)) {
		varDeclaration();
	}
	else {
		statement();
	}

	if (parser.panicMode) synchronize();
}

static void statement()
{
	if (match(TOKEN_PUTS)) {
		putsStatement();
	}
	else {
		expressionStatement();
	}
}

bool aup_compile(AUP_VM, const char *source, aupCh *chunk)
{
	aupLx_init(source);
	compilingChunk = chunk;
	runningVM = vm;

	parser.hadError = false;
	parser.panicMode = false;

	advance();

	while (!match(TOKEN_EOF)) {
		declaration();
	}

	endCompiler();
	return !parser.hadError;
}