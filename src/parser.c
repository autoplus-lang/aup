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

#define REG	int
typedef void (* ParseFn)(REG dest, bool canAssign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

typedef struct {
	aupTk name;
	int depth;
	bool isCaptured;
} Local;

typedef struct {
	uint8_t index;
	bool isLocal;
} Upvalue;

typedef enum {
	TYPE_FUNCTION,
	TYPE_SCRIPT
} FunType;

typedef struct Compiler {
	struct Compiler *enclosing;
	aupOf *function;
	FunType type;

	Local locals[AUP_MAX_LOCALS];
	int localCount;
	Upvalue upvalues[AUP_MAX_LOCALS];
	int scopeDepth;
} Compiler;

static aupPs parser;
static Compiler *current = NULL;
static aupVM *runningVM;

static aupCh *currentChunk()
{
	return &current->function->chunk;
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

static  REG		currentReg;
#define PUSH()	currentReg++
#define POP()	--currentReg
#define POPN(n) currentReg -= (n)
#define RESET()	currentReg = current->localCount

static void emit(uint32_t i)
{
	aupCh *ch = currentChunk();
	if (ch->count <= 0)
		goto _emit;

#define PREV(i)		ch->code[ch->count-(i)]
#define PREV_Op(i)	AUP_GET_Op(PREV(i))
#define PREV_A(i)	AUP_GET_A(PREV(i))
#define PREV_Bx(i)	AUP_GET_Bx(PREV(i))
#define PREV_Cx(i)	AUP_GET_Cx(PREV(i))
#define PREV_sB(i)  AUP_GET_sB(PREV(i))
#define PREV_sC(i)  AUP_GET_sC(PREV(i))

#define GET_Op()	AUP_GET_Op(i)
#define GET_A()		AUP_GET_A(i)
#define GET_Bx()	AUP_GET_Bx(i)
#define GET_Cx()	AUP_GET_Cx(i)
#define GET_sB()	AUP_GET_sB(i)
#define GET_sC()	AUP_GET_sC(i)

#define CODE(x)		AUP_OP_##x
#define UNDO()		ch->count--

	switch (GET_Op())
	{
		case CODE(RET):
		{
			if (GET_A() == true &&
				PREV_Op(1) == CODE(LD))
			{
				i = AUP_SET_OpABx(GET_Op(), GET_A(), PREV_Bx(1));
				UNDO();
			}
			break;
		}
		case CODE(NOT): case CODE(NEG):
		{
			if (PREV_Op(1) == CODE(LD))
			{
				i = AUP_SET_OpABx(GET_Op(), GET_A(), PREV_Bx(1));
				UNDO();
			}
			break;
		}
		case CODE(LT): case CODE(LE): case CODE(EQ):
		case CODE(ADD): case CODE(SUB): case CODE(MUL): case CODE(DIV): case CODE(MOD):
		{
			if (PREV_Op(1) == CODE(LD) &&
				PREV_Op(2) == CODE(LD))
			{
				i = AUP_SET_OpABxCx(GET_Op(), GET_A(), PREV_Bx(2), PREV_Bx(1));
				UNDO(), UNDO();
			}
			else if (PREV_Op(1) == CODE(LD))
			{
				i = AUP_SET_OpABxCx(GET_Op(), GET_A(), GET_Bx(), PREV_Bx(1));
				UNDO();
			}
			break;
		}
	}

_emit:
	aupCh_write(currentChunk(), i,
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

static uint8_t emitConstant(aupV value, REG dest)
{
	uint8_t k = makeConstant(value);
	EMIT_OpABx(LD, dest, k + 256);
	return k;
}

static void emitReturn(REG src)
{
	aupCh *chunk = currentChunk();

	if (chunk->count == 0 || AUP_GET_Op(chunk->code[chunk->count - 1]) != AUP_OP_RET) {
		if (src == -1) {
			EMIT_Op(RET);
		}
		else {
			EMIT_OpABx(RET, true, src);
		}
	}
}

static int emitJump(bool isJMPF, REG srcJMPF)
{
	if (isJMPF)
		EMIT_OpAxCx(JMPF, 0, srcJMPF);
	else
		EMIT_OpAx(JMP, 0);

	return currentChunk()->count - 1;
}

static void patchJump(int offset)
{
	// -1, backtrack after [ip++].
	int jump = currentChunk()->count - offset - 1;

	if (jump > INT16_MAX || jump < INT16_MIN) {
		error("Too much code to jump over.");
	}

	uint32_t *inst = &currentChunk()->code[offset];
	uint8_t op = AUP_GET_Op(*inst);
	int Cx = AUP_GET_Cx(*inst);

	// Patch the hole.
	*inst = AUP_SET_OpAxCx(op, jump, Cx);
}

static void initCompiler(Compiler *compiler, FunType type)
{
	compiler->enclosing = current;
	compiler->function = NULL;
	compiler->type = type;
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	compiler->function = aupOf_new(runningVM);

	current = compiler;

	if (type != TYPE_SCRIPT) {
		current->function->name = aupOs_copy(runningVM, parser.previous.start,
			parser.previous.length);
	}

	Local *local = &current->locals[current->localCount++];
	local->depth = 0;
	local->isCaptured = false;
	local->name.start = "";
	local->name.length = 0;
}

static aupOf *endCompiler()
{
	emitReturn(-1);
	aupOf *function = current->function;

	if (!parser.hadError) {
		aupCh_dasm(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
	}

	current = current->enclosing;
	return function;
}

static void beginScope()
{
	current->scopeDepth++;
}

static void endScope()
{
	current->scopeDepth--;

	while (current->localCount > 0 &&
		current->locals[current->localCount - 1].depth >
		current->scopeDepth) {
		if (current->locals[current->localCount - 1].isCaptured) {
			EMIT_OpA(CLU, current->localCount-1);
		}
		else {
			//emitByte(OP_POP);
		}
		current->localCount--;
	}
}

static struct {
    bool hadCall;
    bool hadAssignment;
} exprInfo;

static REG expression(REG dest);
static void statement();
static void declaration();
static ParseRule *getRule(aupTkt type);
static REG parsePrecedence(Precedence precedence, REG dest);

static uint8_t identifierConstant(aupTk *name)
{
	aupOs *identifier = aupOs_copy(runningVM, name->start, name->length);
	return makeConstant(AUP_OBJ(identifier));
}

static bool identifiersEqual(aupTk *a, aupTk *b)
{
	if (a->length != b->length) return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, aupTk *name)
{
	for (int i = compiler->localCount - 1; i >= 0; i--) {
		Local *local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name)) {
			if (local->depth == -1) {
				error("Cannot read local variable in its own initializer.");
			}
			return i;
		}
	}

	return -1;
}

static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal)
{
	int upvalueCount = compiler->function->upvalueCount;

	for (int i = 0; i < upvalueCount; i++) {
		Upvalue *upvalue = &compiler->upvalues[i];
		if (upvalue->index == index && upvalue->isLocal == isLocal) {
			return i;
		}
	}

	if (upvalueCount == AUP_MAX_ARGS) {
		error("Too many closure variables in function.");
		return 0;
	}

	compiler->upvalues[upvalueCount].isLocal = isLocal;
	compiler->upvalues[upvalueCount].index = index;
	return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, aupTk *name)
{
	if (compiler->enclosing == NULL) return -1;

	int local = resolveLocal(compiler->enclosing, name);
	if (local != -1) {
		compiler->enclosing->locals[local].isCaptured = true;
		return addUpvalue(compiler, (uint8_t)local, true);
	}

	int upvalue = resolveUpvalue(compiler->enclosing, name);
	if (upvalue != -1) {
		return addUpvalue(compiler, (uint8_t)upvalue, false);
	}

	return -1;
}

static void addLocal(aupTk name)
{
	if (current->localCount == AUP_MAX_LOCALS) {
		error("Too many local variables in function.");
		return;
	}

	Local *local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;
	local->isCaptured = false;
}

static void declareVariable()
{
	// Global variables are implicitly declared.
	if (current->scopeDepth == 0) return;

	aupTk *name = &parser.previous;
	for (int i = current->localCount - 1; i >= 0; i--) {
		Local *local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth) {
			break;
		}

		if (identifiersEqual(name, &local->name)) {
			error("The variable already declared in this scope.");
		}
	}

	addLocal(*name);
}

static uint8_t parseVariable(const char *errorMessage)
{
	consume(TOKEN_IDENTIFIER, errorMessage);

	declareVariable();
	if (current->scopeDepth > 0) return 0;

	return identifierConstant(&parser.previous);
}

static void markInitialized()
{
	if (current->scopeDepth == 0) return;

	current->locals[current->localCount - 1].depth =
		current->scopeDepth;
}

static void defineVariable(uint8_t global, REG src)
{
	if (current->scopeDepth > 0) {
		markInitialized();
		return;
	}

	if (src == -1)
		EMIT_OpAsC(GST, global, true);
	else
		EMIT_OpABx(GST, global, src);
}

static uint8_t argumentList()
{
	uint8_t argCount = 0;
	if (!check(TOKEN_RPAREN)) {
		do {
			expression(-1);
			if (++argCount >= AUP_MAX_ARGS) {
				error("Cannot have more than %d arguments.", AUP_MAX_ARGS);
			}
		} while (match(TOKEN_COMMA));
	}

	consume(TOKEN_RPAREN, "Expect ')' after arguments.");
	return argCount;
}

static void and_(REG dest, bool canAssign)
{
	int endJump = emitJump(true, dest);

	//emitByte(OP_POP);
	parsePrecedence(PREC_AND, dest);

	patchJump(endJump);
}

static void binary(REG dest, bool canAssign)
{
	REG left = dest;

	// Remember the operator.                                
	aupTkt operatorType = parser.previous.type;

	// Compile the right operand.                            
	ParseRule *rule = getRule(operatorType);
	REG right = parsePrecedence((Precedence)(rule->precedence + 1), -1);
	POP();

	// Emit the operator instruction.                        
	switch (operatorType) {
		case TOKEN_EQUAL_EQ:	EMIT_OpABxCx(EQ, dest, left, right); break;
		case TOKEN_LESS:		EMIT_OpABxCx(LT, dest, left, right); break;
		case TOKEN_LESS_EQ:		EMIT_OpABxCx(LE, dest, left, right); break;

		case TOKEN_BANG_EQ:		EMIT_OpABxCx(EQ, dest, left, right); EMIT_OpABx(NOT, dest, dest); break;
		case TOKEN_GREATER:		EMIT_OpABxCx(LE, dest, left, right); EMIT_OpABx(NOT, dest, dest); break;
		case TOKEN_GREATER_EQ:	EMIT_OpABxCx(LT, dest, left, right); EMIT_OpABx(NOT, dest, dest); break;

		case TOKEN_PLUS:		EMIT_OpABxCx(ADD, dest, left, right); break;
		case TOKEN_MINUS:		EMIT_OpABxCx(SUB, dest, left, right); break;
		case TOKEN_STAR:		EMIT_OpABxCx(MUL, dest, left, right); break;
		case TOKEN_SLASH:		EMIT_OpABxCx(DIV, dest, left, right); break;
		case TOKEN_PERCENT:		EMIT_OpABxCx(MOD, dest, left, right); break;
	}
}

static void call(REG dest, bool canAssign)
{
	uint8_t argCount = argumentList();
	EMIT_OpAB(CALL, dest, argCount);

	POPN(argCount);
    exprInfo.hadCall = true;
}

static void literal(REG dest, bool canAssign)
{
	switch (parser.previous.type) {
		case TOKEN_NIL:		EMIT_OpA(NIL, dest); break;
		case TOKEN_FALSE:	EMIT_OpAsB(BOOL, dest, 0); break;
		case TOKEN_TRUE:	EMIT_OpAsB(BOOL, dest, 1); break;
		case TOKEN_FUNC:     EMIT_OpAB(MOV, dest, 0); break;             
	}
}

static void grouping(REG dest, bool canAssign)
{
	expression(dest);
	consume(TOKEN_RPAREN, "Expect ')' after expression.");
}

static void or_(REG dest, bool canAssign)
{
	int elseJump = emitJump(true, dest);
	int endJump = emitJump(false, -1);

	patchJump(elseJump);
	//emitByte(OP_POP);

	parsePrecedence(PREC_OR, dest);
	patchJump(endJump);
}

static void number(REG dest, bool canAssign)
{
	double value = strtod(parser.previous.start, NULL);

	emitConstant(AUP_NUM(value), dest);
}

static void integer(REG dest, bool canAssign)
{
	int64_t value; int radix;

	switch (parser.previous.type) {
		case TOKEN_BINARY:		radix = 2; goto _m;
		case TOKEN_OCTAL:		radix = 8; goto _m;
		case TOKEN_HEXADECIMAL:	radix = 16;
		_m: value = strtoll(parser.previous.start + 2, NULL, radix);
			break;
		case TOKEN_INTEGER:
			value = strtoll(parser.previous.start, NULL, 0);
			break;
	}

	emitConstant(AUP_INT(value), dest);
}

static void string(REG dest, bool canAssign)
{
	aupOs *value = aupOs_copy(runningVM,
		parser.previous.start + 1, parser.previous.length - 2);

	emitConstant(AUP_OBJ(value), dest);
}

static void namedVariable(aupTk name, REG dest, bool canAssign)
{
    aupOp loadOp, storeOp;
	REG arg = resolveLocal(current, &name);

    if (arg != -1) {
        loadOp = CODE(LD);
        storeOp = CODE(ST);
    }
    else if ((arg = resolveUpvalue(current, &name)) != -1) {
        loadOp = CODE(ULD);
        storeOp = CODE(UST);
    }
    else {
        arg = identifierConstant(&name);
        loadOp = CODE(GLD);
        storeOp = CODE(GST);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        REG src = expression(dest);
        emit(AUP_SET_OpABx(storeOp, arg, src));

        exprInfo.hadAssignment = true;
    }
    else if (canAssign && match(TOKEN_PLUS_EQ)) {
        REG src = PUSH();
        namedVariable(name, src, false);
        REG exp = expression(-1); POP();
        EMIT_OpABxCx(ADD, dest, src, exp);
        emit(AUP_SET_OpABx(storeOp, arg, dest));
        
        exprInfo.hadAssignment = true;
    }
    else if (canAssign && match(TOKEN_MINUS_EQ)) {
        REG src = PUSH();
        namedVariable(name, src, false);
        REG exp = expression(-1); POP();
        EMIT_OpABxCx(SUB, dest, src, exp);
        emit(AUP_SET_OpABx(storeOp, arg, dest));

        exprInfo.hadAssignment = true;
    }
    else if (canAssign && match(TOKEN_STAR_EQ)) {
        REG src = PUSH();
        namedVariable(name, src, false);
        REG exp = expression(-1); POP();
        EMIT_OpABxCx(MUL, dest, src, exp);
        emit(AUP_SET_OpABx(storeOp, arg, dest));

        exprInfo.hadAssignment = true;
    }
    else if (canAssign && match(TOKEN_SLASH_EQ)) {
        REG src = PUSH();
        namedVariable(name, src, false);
        REG exp = expression(-1); POP();
        EMIT_OpABxCx(DIV, dest, src, exp);
        emit(AUP_SET_OpABx(storeOp, arg, dest));

        exprInfo.hadAssignment = true;
    }
    else if (canAssign && match(TOKEN_PERCENT_EQ)) {
        REG src = PUSH();
        namedVariable(name, src, false);
        REG exp = expression(-1); POP();
        EMIT_OpABxCx(MOD, dest, src, exp);
        emit(AUP_SET_OpABx(storeOp, arg, dest));

        exprInfo.hadAssignment = true;
    }
    else {
        emit(AUP_SET_OpABx(loadOp, dest, arg));
    }
}

static void variable(REG dest, bool canAssign)
{
	namedVariable(parser.previous, dest, canAssign);
}

static void unary(REG dest, bool canAssign)
{
	aupTkt operatorType = parser.previous.type;

	// Compile the operand.                        
	REG src = parsePrecedence(PREC_UNARY, dest);

	// Emit the operator instruction.              
	switch (operatorType) {
		case TOKEN_BANG:	EMIT_OpABx(NOT, dest, src); break;
		case TOKEN_MINUS:	EMIT_OpABx(NEG, dest, src); break;
	}
}

static ParseRule rules[] = {
    [TOKEN_LPAREN]          = { grouping, call,    PREC_CALL },
    [TOKEN_RPAREN]          = { NULL,     NULL,    PREC_NONE },
    [TOKEN_LBRACKET]        = { NULL,     NULL,    PREC_NONE },
    [TOKEN_RBRACKET]        = { NULL,     NULL,    PREC_NONE },
    [TOKEN_LBRACE]          = { NULL,     NULL,    PREC_NONE },
    [TOKEN_RBRACE]          = { NULL,     NULL,    PREC_NONE },

    [TOKEN_COMMA]           = { NULL,     NULL,    PREC_NONE },
    [TOKEN_DOT]             = { NULL,     NULL,    PREC_NONE },
    [TOKEN_COLON]           = { NULL,     NULL,    PREC_NONE },
    [TOKEN_SEMICOLON]       = { NULL,     NULL,    PREC_NONE },

    [TOKEN_MINUS]           = { unary,    binary,  PREC_TERM },
    [TOKEN_PLUS]            = { NULL,     binary,  PREC_TERM },
    [TOKEN_SLASH]           = { NULL,     binary,  PREC_FACTOR },
    [TOKEN_STAR]            = { NULL,     binary,  PREC_FACTOR },
    [TOKEN_PERCENT]         = { NULL,     binary,  PREC_FACTOR },

    [TOKEN_BANG]            = { unary,    NULL,    PREC_NONE },
    [TOKEN_BANG_EQ]         = { NULL,     binary,  PREC_EQUALITY },
    [TOKEN_EQUAL]           = { NULL,     NULL,    PREC_NONE },
    [TOKEN_EQUAL_EQ]        = { NULL,     binary,  PREC_EQUALITY },
    [TOKEN_GREATER]         = { NULL,     binary,  PREC_COMPARISON },
    [TOKEN_GREATER_EQ]      = { NULL,     binary,  PREC_COMPARISON },
    [TOKEN_LESS]            = { NULL,     binary,  PREC_COMPARISON },
    [TOKEN_LESS_EQ]         = { NULL,     binary,  PREC_COMPARISON },

    [TOKEN_IDENTIFIER]      = { variable, NULL,    PREC_NONE },
    [TOKEN_STRING]          = { string,   NULL,    PREC_NONE },
    [TOKEN_NUMBER]          = { number,   NULL,    PREC_NONE },
    [TOKEN_INTEGER]         = { integer,  NULL,    PREC_NONE },
    [TOKEN_BINARY]          = { integer,  NULL,    PREC_NONE },
    [TOKEN_OCTAL]           = { integer,  NULL,    PREC_NONE },
    [TOKEN_HEXADECIMAL]     = { integer,  NULL,    PREC_NONE },

    [TOKEN_AND]             = { NULL,     and_,    PREC_AND  },
    [TOKEN_CLASS]           = { NULL,     NULL,    PREC_NONE },
    [TOKEN_ELSE]            = { NULL,     NULL,    PREC_NONE },
    [TOKEN_END]             = { NULL,     NULL,    PREC_NONE },
    [TOKEN_FALSE]           = { literal,  NULL,    PREC_NONE },
    [TOKEN_FOR]             = { NULL,     NULL,    PREC_NONE },
    [TOKEN_FUNC]            = { literal,  NULL,    PREC_NONE },
    [TOKEN_IF]              = { NULL,     NULL,    PREC_NONE },
    [TOKEN_NIL]             = { literal,  NULL,    PREC_NONE },
    [TOKEN_OR]              = { NULL,     or_,     PREC_OR   },
    [TOKEN_PUTS]            = { NULL,     NULL,    PREC_NONE },
    [TOKEN_RETURN]          = { NULL,     NULL,    PREC_NONE },
    [TOKEN_SUPER]           = { NULL,     NULL,    PREC_NONE },
    [TOKEN_THEN]            = { NULL,     NULL,    PREC_NONE },
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

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(dest, canAssign);

	while (precedence <= getRule(parser.current.type)->precedence) {
        if (parser.current.line > parser.previous.line) {
            // Break expression on new line.
            break;
        }
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(dest, canAssign);
	}

	if (canAssign && match(TOKEN_EQUAL)) {
		error("Invalid assignment target.");
		return -1;
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

static void block()
{
	while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RBRACE, "Expect '}' after block.");
}

static void function(FunType type, REG dest)
{
	Compiler compiler;
	initCompiler(&compiler, type);
	beginScope();

	// Compile the parameter list.                                
	consume(TOKEN_LPAREN, "Expect '(' after function name.");
	if (!check(TOKEN_RPAREN)) {
		do {
			current->function->arity++;
			if (current->function->arity > 255) {
				errorAtCurrent("Cannot have more than 255 parameters.");
			}

			uint8_t paramConstant = parseVariable("Expect parameter name.");
			defineVariable(paramConstant, -1);
		} while (match(TOKEN_COMMA));
	}
	consume(TOKEN_RPAREN, "Expect ')' after parameters.");

	// The body. 

    if (match(TOKEN_EQUAL)) {
        REG src = expression(-1);
        emitReturn(src);
    }
    else {
        consume(TOKEN_LBRACE, "Expect '{' before function body.");
        block();
    }

	// Create the function object.                                
	aupOf *function = endCompiler();
	
	uint8_t k = makeConstant(AUP_OBJ(function));

    // Make closure if upvalues exist.
    if (function->upvalueCount > 0) {
        EMIT_OpA(CLO, k);
        for (int i = 0; i < function->upvalueCount; i++) {
            EMIT_OpAsB(NOP, compiler.upvalues[i].index,
                compiler.upvalues[i].isLocal);
        }
    }

	EMIT_OpABx(LD, dest, k + 256);
}

static void funDeclaration()
{
	uint8_t global = parseVariable("Expect function name.");
	markInitialized();
	REG src = PUSH();
	function(TYPE_FUNCTION, src);
	defineVariable(global, src);
}

static void varDeclaration()
{
	uint8_t global = parseVariable("Expect variable name.");
	REG src;

	if (match(TOKEN_EQUAL)) {
		src = expression(-1);
		POP();
	}
	else {
		src = -1; //emitByte(OP_NIL);
	}

	defineVariable(global, src);
}

static void expressionStatement()
{
    memset(&exprInfo, 0, sizeof(exprInfo));

    expression(-1);
    POP();

    if (!exprInfo.hadCall &&
        !exprInfo.hadAssignment) {
        error("Unexpected expression.");
    }
}

static void ifStatement()
{
    bool hadParen = match(TOKEN_LPAREN);
    REG src = expression(-1);
    if (hadParen) {
        consume(TOKEN_RPAREN, "Expect ')' after condition.");
    }
    else if (!check(TOKEN_THEN)) {
        error("Expect 'then' after condition.");
    }

	int thenJump = emitJump(true, src);
	POP();
	statement();

	if (match(TOKEN_ELSE)) {
		int elseJump = emitJump(false, -1);
		patchJump(thenJump);
		statement();
		patchJump(elseJump);
	}
	else {
		patchJump(thenJump);
	}
}

static void putsStatement()
{
	int count = 1;
	REG src = expression(-1);

	while (match(TOKEN_COMMA)) {
		expression(-1);
		if (++count > AUP_MAX_ARGS) {
			error("Too many values in 'puts' statement.");
			return;
		}
	};

	EMIT_OpAB(PUT, src, count), POPN(count);
}

static void returnStatement()
{
	if (current->type == TYPE_SCRIPT) {
		error("Cannot return from top-level code.");
	}

	if (match(TOKEN_SEMICOLON)
		|| check(TOKEN_RBRACE)) {
        emitReturn(-1);
	}
	else {
		REG src = expression(-1);
        emitReturn(src);
	}
}

static void synchronize()
{
	parser.panicMode = false;

	while (parser.current.type != TOKEN_EOF) {
		if (parser.previous.type == TOKEN_SEMICOLON) return;

		switch (parser.current.type) {
			case TOKEN_CLASS:
			case TOKEN_FUNC:
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
	RESET();

	if (match(TOKEN_FUNC)) {
		funDeclaration();
	}
	else if (match(TOKEN_VAR)) {
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
	else if (match(TOKEN_IF)) {
		ifStatement();
	}
	else if (match(TOKEN_RETURN)) {
		returnStatement();
	}
    else if (match(TOKEN_THEN)) {
        beginScope();
        if (parser.current.line == parser.previous.line) {
            if (!check(TOKEN_END)) statement();
            match(TOKEN_END);
        }
        else {
            while (!check(TOKEN_END) && !check(TOKEN_EOF)) {
                declaration();
            }
            consume(TOKEN_END, "Expect 'end' after block.");
        }
        endScope();
    }
	else if (match(TOKEN_LBRACE)) {
		beginScope();
		block();
		endScope();
	}
    else if (match(TOKEN_SEMICOLON)) {
        // Do nothing.
    }
    else {
        expressionStatement();
    }
}

aupOf *aup_compile(AUP_VM, const char *source)
{
	runningVM = vm;
	aupLx_init(source);

	Compiler compiler;
	initCompiler(&compiler, TYPE_SCRIPT);

	parser.hadError = false;
	parser.panicMode = false;

	advance();
	if (!match(TOKEN_EOF)) {
		do {
			declaration();
		} while (!match(TOKEN_EOF));
	}

	aupOf *function = endCompiler();
	return parser.hadError ? NULL : function;
}
