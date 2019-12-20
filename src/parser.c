#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"

typedef struct {
	aupTk current;
	aupTk previous;
	bool hadError;
	bool panicMode;
} aupPs;

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

static void endCompiler()
{
	//emitReturn();
}

static void expression()
{
	// What goes here?      
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