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
typedef void (* ParseFn)(REG dest, bool canAssign);

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

typedef struct {
	aupTk name;
	int depth;
} Local;

typedef struct Compiler {
	Local locals[AUP_MAX_LOCALS];
	int localCount;
	int scopeDepth;
} Compiler;

static aupPs parser;
static Compiler *current = NULL;
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
#define POPN(n) currentReg -= (n)
#define RESET()	currentReg = current->localCount

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

static void emitConstant(aupV value, REG dest)
{
	uint8_t k = makeConstant(value);
	EMIT_OpAB(LDK, dest, k);
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
	// -1, backtrack after [ip++]
	int jump = currentChunk()->count - offset - 1;

	if (jump > INT16_MAX || jump < INT16_MIN) {
		error("Too much code to jump over.");
	}

	uint32_t *inst = &currentChunk()->code[offset];

	uint8_t op = AUP_GET_Op(*inst);
	int Cx = AUP_GET_Cx(*inst);
	*inst = AUP_SET_OpAxCx(op, jump, Cx);
}

static void initCompiler(Compiler *compiler)
{
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	current = compiler;
}

static void endCompiler()
{
	EMIT_Op(RET);

	if (!parser.hadError) {
		aupCh_dasm(currentChunk(), "code");
	}
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
		//emitByte(OP_POP);
		current->localCount--;
	}
}

static REG expression(REG dest);
static void statement();
static void declaration();
static ParseRule *getRule(aupTkt type);
static REG parsePrecedence(Precedence precedence, REG dest);

static uint8_t identifierConstant(aupTk *name)
{
	aupOs *identifier = aupOs_copy(currentVM(), name->start, name->length);
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

static void addLocal(aupTk name)
{
	if (current->localCount == AUP_MAX_LOCALS) {
		error("Too many local variables in function.");
		return;
	}

	Local *local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;
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
			error("Variable with this name already declared in this scope.");
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
		EMIT_OpAsB(DEF, global, true);
	else
		EMIT_OpAB(DEF, global, src);
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
	ParseRule* rule = getRule(operatorType);
	REG right = parsePrecedence((Precedence)(rule->precedence + 1), -1);
	POP();

	// Emit the operator instruction.                        
	switch (operatorType) {
		case TOKEN_EQUAL_EQUAL:		EMIT_OpABC(EQ, dest, left, right); break;	//emitByte(OP_EQUAL); break;
		case TOKEN_LESS:			EMIT_OpABC(LT, dest, left, right); break;	//emitByte(OP_LESS); break;
		case TOKEN_LESS_EQUAL:		EMIT_OpABC(LE, dest, left, right); break;	//emitBytes(OP_GREATER, OP_NOT); break;

		case TOKEN_BANG_EQUAL:		EMIT_OpABC(EQ, dest, left, right);
									EMIT_OpAB(NOT, dest, dest); break;			//emitBytes(OP_EQUAL, OP_NOT); break;
		case TOKEN_GREATER:			EMIT_OpABC(LE, dest, left, right);
									EMIT_OpAB(NOT, dest, dest); break;			//emitByte(OP_GREATER); break;
		case TOKEN_GREATER_EQUAL:	EMIT_OpABC(LT, dest, left, right);
									EMIT_OpAB(NOT, dest, dest); break;			//emitBytes(OP_LESS, OP_NOT); break;

		case TOKEN_PLUS:			EMIT_OpABC(ADD, dest, left, right); break;	//emitByte(OP_ADD); break;
		case TOKEN_MINUS:			EMIT_OpABC(SUB, dest, left, right); break;	//emitByte(OP_SUBTRACT); break;
		case TOKEN_STAR:			EMIT_OpABC(MUL, dest, left, right); break;	//emitByte(OP_MULTIPLY); break;
		case TOKEN_SLASH:			EMIT_OpABC(DIV, dest, left, right); break;	//emitByte(OP_DIVIDE); break;
		case TOKEN_PERCENT:			EMIT_OpABC(MOD, dest, left, right); break;

		default: return; // Unreachable.                              
	}
}

static void literal(REG dest, bool canAssign)
{
	switch (parser.previous.type) {
		case TOKEN_NIL:		EMIT_OpA(NIL, dest); break;			//emitByte(OP_NIL); break;
		case TOKEN_FALSE:	EMIT_OpAsB(BOL, dest, 0); break;	//emitByte(OP_FALSE); break;
		case TOKEN_TRUE:	EMIT_OpAsB(BOL, dest, 1); break;	//emitByte(OP_TRUE); break;

		default: return; // Unreachable.                   
	}
}

static void grouping(REG dest, bool canAssign)
{
	expression(dest);
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(REG dest, bool canAssign)
{
	double value;

	switch (parser.previous.type) {
		case TOKEN_BINARY:
			value = strtol(parser.previous.start + 2, NULL, 2);
			break;
		case TOKEN_OCTAL:
			value = strtol(parser.previous.start + 2, NULL, 8);
			break;
		case TOKEN_HEXADECIMAL:
			value = strtol(parser.previous.start + 2, NULL, 16);
			break;
		case TOKEN_NUMBER: default:
			value = strtod(parser.previous.start, NULL);
			break;
	}

	emitConstant(AUP_NUM(value), dest);
}

static void string(REG dest, bool canAssign)
{
	aupOs *value = aupOs_copy(currentVM(),
		parser.previous.start + 1, parser.previous.length - 2);

	emitConstant(AUP_OBJ(value), dest);
}

static void namedVariable(aupTk name, REG dest, bool canAssign)
{
	int arg = resolveLocal(current, &name);

	if (arg != -1) {
		if (canAssign && match(TOKEN_EQUAL)) {
			REG src = expression(dest);
			EMIT_OpAB(ST, arg, src);	//setOp = OP_SET_LOCAL;
		}
		else {
			EMIT_OpAB(LD, dest, arg);	//getOp = OP_GET_LOCAL;
		}
	}
	else {
		arg = identifierConstant(&name);
		if (canAssign && match(TOKEN_EQUAL)) {
			REG src = expression(dest);
			EMIT_OpAB(GST, arg, src);	//emitBytes(OP_SET_GLOBAL, arg);
		}
		else {
			EMIT_OpAB(GLD, dest, arg);	//emitBytes(OP_GET_GLOBAL, arg);
		}
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
		case TOKEN_BANG:	EMIT_OpAB(NOT, dest, src); break;	//emitByte(OP_NOT);
		case TOKEN_MINUS:	EMIT_OpAB(NEG, dest, src); break;	//emitByte(OP_NEGATE);
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
    [TOKEN_PERCENT]         = { NULL,     binary,  PREC_FACTOR },

    [TOKEN_BANG]            = { unary,    NULL,    PREC_NONE },
    [TOKEN_BANG_EQUAL]      = { NULL,     binary,  PREC_EQUALITY },
    [TOKEN_EQUAL]           = { NULL,     NULL,    PREC_NONE },
    [TOKEN_EQUAL_EQUAL]     = { NULL,     binary,  PREC_EQUALITY },
    [TOKEN_GREATER]         = { NULL,     binary,  PREC_COMPARISON },
    [TOKEN_GREATER_EQUAL]   = { NULL,     binary,  PREC_COMPARISON },
    [TOKEN_LESS]            = { NULL,     binary,  PREC_COMPARISON },
    [TOKEN_LESS_EQUAL]      = { NULL,     binary,  PREC_COMPARISON },

    [TOKEN_IDENTIFIER]      = { variable, NULL,    PREC_NONE },
    [TOKEN_STRING]          = { string,   NULL,    PREC_NONE },
    [TOKEN_NUMBER]          = { number,   NULL,    PREC_NONE },
    [TOKEN_BINARY]          = { number,   NULL,    PREC_NONE },
    [TOKEN_OCTAL]           = { number,   NULL,    PREC_NONE },
    [TOKEN_HEXADECIMAL]     = { number,   NULL,    PREC_NONE },

    [TOKEN_AND]             = { NULL,     and_,    PREC_AND  },
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

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(dest, canAssign);

	while (precedence <= getRule(parser.current.type)->precedence) {
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
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
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
	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

	defineVariable(global, src);
}

static void expressionStatement()
{
	expression(-1);
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");

	POP();
}

static void ifStatement()
{
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	REG src = expression(-1);
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int thenJump = emitJump(true, src);
	POP();	//emitByte(OP_POP);
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
	int nvalues = 1;
	REG src = expression(-1);

	while (match(TOKEN_COMMA)) {
		expression(-1);
		if (++nvalues > AUP_MAX_ARGS) {
			error("Too many values in 'puts' statement.");
			return;
		}
	};

	consume(TOKEN_SEMICOLON, "Expect ';' after value.");

	EMIT_OpAB(PUT, src, nvalues), POPN(nvalues);
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
	RESET();

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
	else if (match(TOKEN_IF)) {
		ifStatement();
	}
	else if (match(TOKEN_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
	}
	else {
		expressionStatement();
	}
}

bool aup_compile(AUP_VM, const char *source, aupCh *chunk)
{
	aupLx_init(source);
	Compiler compiler;
	initCompiler(&compiler);

	compilingChunk = chunk;
	runningVM = vm;

	parser.hadError = false;
	parser.panicMode = false;

	advance();

	if (!match(TOKEN_EOF)) {
		do {
			declaration();
		} while (!match(TOKEN_EOF));
	}

	endCompiler();
	return !parser.hadError;
}
