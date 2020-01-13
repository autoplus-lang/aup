#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "code.h"
#include "object.h"
#include "vm.h"
#include "gc.h"

#define MAX_ARGS    64
#define MAX_CASES   UINT8_COUNT

typedef struct _aupCompiler Compiler;

typedef struct {
    aupVM *vm;
    aupLexer *lexer;
    aupSrc *source;
    Compiler *compiler;

    aupTok current;
    aupTok previous;
    bool hadError;
    bool panicMode;

    bool hadCall;
    bool hadAssign;
    int subExprs;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_TERNARY,     // ?:
    PREC_OR,          // or       
    PREC_AND,         // and
    PREC_BOR,		  // |
    PREC_BXOR,		  // ^
    PREC_BAND,		  // &
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_SHIFT,       // << >>
    PREC_TERM,        // + -      
    PREC_FACTOR,      // * /      
    PREC_UNARY,       // ! - ~
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

typedef void (* ParseFn)(Parser *P, bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    aupTok name;
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

typedef struct {
    int start;
    int breakCount;
    int breaks[UINT8_COUNT];
} Loop;

struct _aupCompiler {
    Compiler *enclosing;
    aupFun *function;
    FunType type;
    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;
    Loop *currentLoop;
    int loopDepth;
    bool ifNeedEnd;
};

static aupChunk *currentChunk(Parser *P)
{
    return &P->compiler->function->chunk;
}

static void errorAt(Parser *P, aupTok *token, const char *fmt, ...)
{
    if (P->panicMode) return;
    P->panicMode = true;

    fprintf(stderr, "[%s:%d:%d] Error", P->source->fname, token->line, token->column);

    if (token->type == AUP_TOK_EOF) {
        fprintf(stderr, " at end");
    }
    else if (token->type == AUP_TOK_ERROR) {
        // Nothing.                                                
    }
    else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    va_list argp;
    va_start(argp, fmt);
    fprintf(stderr, ": ");
    vfprintf(stderr, fmt, argp);
    fprintf(stderr, "\n");
    va_end(argp);

    int padding = snprintf(NULL, 0, "%3d", token->line);
    fprintf(stderr, " %*s |\n", padding, "");
    fprintf(stderr, " %*d | %.*s\n", padding, token->line, token->lineLength, token->lineStart);
    fprintf(stderr, " %*s |%*s", padding, "", token->column, "");
    for (int i = 0; i < token->length; i++) fputc('^', stderr);
    fprintf(stderr, "\n");

    fflush(stderr);
    P->hadError = true; 
}

#define error(P, fmt, ...)          errorAt(P, &(P)->previous, fmt, ##__VA_ARGS__)
#define errorAtCurrent(P, fmt, ...) errorAt(P, &(P)->current, fmt, ##__VA_ARGS__)
#define consume(P, toktype, fmt, ...) \
    do { \
		if ((P)->current.type == (toktype)) { \
			advance(P); \
			break; \
		} \
		errorAtCurrent(P, fmt, ##__VA_ARGS__); \
	} while (0)

static void advance(Parser *P)
{
    P->previous = P->current;

    for (;;) {
        P->current = aup_scanToken(P->lexer);
        if (P->current.type != AUP_TOK_ERROR) break;

        errorAtCurrent(P, P->current.start);
    }
}

static bool check(Parser *P, aupTokType type)
{
    return P->current.type == type;
}

static bool checkPrev(Parser *P, aupTokType type)
{
    return P->previous.type == type;
}

static bool match(Parser *P, aupTokType type)
{
    if (!check(P, type)) return false;
    advance(P);
    return true;
}

static void emitByte(Parser *P, uint8_t byte)
{
    aup_emitChunk(currentChunk(P), byte,
        P->previous.line, P->previous.column);
}

static void emitBytes(Parser *P, uint8_t byte1, uint8_t byte2)
{
    emitByte(P, byte1);
    emitByte(P, byte2);
}

static void emitWord(Parser *P, uint16_t word)
{
    emitBytes(P, (word >> 8) & 0xFF, word & 0xFF);
}

static int emitJump(Parser *P, uint8_t instruction)
{
    emitByte(P, instruction);
    emitBytes(P, 0, 0);
    return currentChunk(P)->count - 2;
}

static void emitLoop(Parser *P, int loopStart)
{
    emitByte(P, AUP_OP_LOOP);

    int offset = currentChunk(P)->count - loopStart + 2;
    if (offset > UINT16_MAX) error(P, "Loop body too large.");

    emitByte(P, (offset >> 8) & 0xff);
    emitByte(P, offset & 0xff);
}

static void emitReturn(Parser *P)
{
    int count;
    if ((count = currentChunk(P)->count) > 0 &&
        currentChunk(P)->code[count - 1] == AUP_OP_RET) {
        return;
    }

    emitByte(P, AUP_OP_NIL);
    emitByte(P, AUP_OP_RET);
}

static uint8_t makeConstant(Parser *P, aupVal value)
{
    bool isObject = AUP_IS_OBJ(value);
    if (isObject) aup_pushRoot(P->vm, AUP_AS_OBJ(value));
    int constant = aup_pushArray(&currentChunk(P)->constants, value, false);
    if (isObject) aup_popRoot(P->vm);

    if (constant > UINT8_MAX) {
        error(P, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitSmart(Parser *P, uint8_t op, int arg)
{
    emitBytes(P, op, (uint8_t)arg);
}

static void emitConstant(Parser *P, aupVal value)
{
    uint8_t constant = makeConstant(P, value);
    emitBytes(P, AUP_OP_CONST, constant);
}

static void patchJump(Parser *P, int offset)
{
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk(P)->count - offset - 2;

    if (jump > UINT16_MAX) {
        error(P, "Too much code to jump over.");
    }

    currentChunk(P)->code[offset] = (jump >> 8) & 0xff;
    currentChunk(P)->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Parser *P, Compiler *compiler, FunType type)
{
    compiler->enclosing = P->compiler;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->loopDepth = 0;
    compiler->currentLoop = NULL;
    compiler->function = aup_newFunction(P->vm, P->source);

    if (type != TYPE_SCRIPT) {
        compiler->function->name = aup_copyString(P->vm, P->previous.start,
            P->previous.length);
    }

    Local *local = &compiler->locals[compiler->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    local->name.start = "";
    local->name.length = 0;

    P->compiler = compiler;
}

static aupFun *endCompiler(Parser *P)
{
    emitReturn(P);
    aupFun *function = P->compiler->function;

#ifdef AUP_DEBUG
    if (!P->hadError) {
        aup_dasmChunk(currentChunk(P), function->name == NULL ? "<script>"
            : function->name->chars);
    }
#endif

    P->compiler = P->compiler->enclosing;
    return function;
}

static void beginScope(Parser *P)
{
    Compiler *current = P->compiler;
    current->scopeDepth++;
}

static void endScope(Parser *P)
{
    Compiler *current = P->compiler;
    current->scopeDepth--;

    while (current->localCount > 0 &&
        current->locals[current->localCount - 1].depth >
        current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(P, AUP_OP_CLOSE);
        }
        else {
            emitByte(P, AUP_OP_POP);
        }
        current->localCount--;
    }
}

static void expression(Parser *P);
static void statement(Parser *P);
static void declaration(Parser *P);
static ParseRule *getRule(aupTokType type);
static void parsePrecedence(Parser *P, Precedence precedence);

static uint8_t identifierConstant(Parser *P, aupTok *name)
{
    aupStr *id = aup_copyString(P->vm, name->start, name->length);
    return makeConstant(P, AUP_OBJ(id));
}

static bool identifiersEqual(aupTok *a, aupTok *b)
{
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Parser *P, Compiler *compiler, aupTok *name)
{
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error(P, "Cannot read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static int addUpvalue(Parser *P, Compiler *compiler, uint8_t index, bool isLocal)
{
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error(P, "Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Parser *P, Compiler *compiler, aupTok* name)
{
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(P, compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(P, compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(P, compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(P, compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Parser *P, aupTok name)
{
    Compiler *current = P->compiler;

    if (current->localCount == UINT8_COUNT) {
        error(P, "Too many local variables in function.");
        return;
    }

    Local *local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

static void declareVariable(Parser *P)
{
    Compiler *current = P->compiler;

    // Global variables are implicitly declared.
    if (current->scopeDepth == 0) return;

    aupTok *name = &P->previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error(P, "Variable with this name already declared in this scope.");
        }
    }

    addLocal(P, *name);
}

static uint8_t parseVariable(Parser *P, const char *errorMessage)
{
    consume(P, AUP_TOK_IDENTIFIER, errorMessage);

    declareVariable(P);
    if (P->compiler->scopeDepth > 0) return 0;

    return identifierConstant(P, &P->previous);
}

static void markInitialized(Parser *P)
{
    Compiler *current = P->compiler;

    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth =
        current->scopeDepth;
}

static void defineVariable(Parser *P, uint8_t global)
{
    if (P->compiler->scopeDepth > 0) {
        markInitialized(P);
        return;
    }

    emitSmart(P, AUP_OP_DEF, global);
}

static uint8_t argumentList(Parser *P)
{
    uint8_t argc = 0;
    if (!check(P, AUP_TOK_RPAREN)) {
        do {
            argc++;
            expression(P);
            if (argc >= MAX_ARGS) {
                error(P, "Cannot have more than %d arguments.", MAX_ARGS);
            }
        } while (match(P, AUP_TOK_COMMA));
    }

    consume(P, AUP_TOK_RPAREN, "Expect ')' after arguments.");
    return argc;
}

static void and_(Parser *P, bool canAssign)
{
    int endJump = emitJump(P, AUP_OP_JMPF);

    emitByte(P, AUP_OP_POP);
    parsePrecedence(P, PREC_AND);

    patchJump(P, endJump);
}

static void binary(Parser *P, bool canAssign)
{
    // Remember the operator.                                
    aupTokType operatorType = P->previous.type;

    // Compile the right operand.                            
    ParseRule *rule = getRule(operatorType);
    parsePrecedence(P, (Precedence)(rule->precedence + 1));

    // Emit the operator instruction.                        
    switch (operatorType) {
        case AUP_TOK_EQUAL_EQUAL:   emitByte(P, AUP_OP_EQ); break;
        case AUP_TOK_LESS:          emitByte(P, AUP_OP_LT); break;
        case AUP_TOK_LESS_EQUAL:    emitByte(P, AUP_OP_LE); break;

        case AUP_TOK_BANG_EQUAL:    emitBytes(P, AUP_OP_EQ, AUP_OP_NOT); break;
        case AUP_TOK_GREATER:       emitBytes(P, AUP_OP_LE, AUP_OP_NOT); break;
        case AUP_TOK_GREATER_EQUAL: emitBytes(P, AUP_OP_LT, AUP_OP_NOT); break;

        case AUP_TOK_PLUS:          emitByte(P, AUP_OP_ADD); break;
        case AUP_TOK_MINUS:         emitByte(P, AUP_OP_SUB); break;
        case AUP_TOK_STAR:          emitByte(P, AUP_OP_MUL); break;
        case AUP_TOK_SLASH:         emitByte(P, AUP_OP_DIV); break;
        case AUP_TOK_PERCENT:       emitByte(P, AUP_OP_MOD); break;

        case AUP_TOK_AMPERSAND:     emitByte(P, AUP_OP_BAND); break;
        case AUP_TOK_VBAR:          emitByte(P, AUP_OP_BOR); break;
        case AUP_TOK_CARET:         emitByte(P, AUP_OP_BXOR); break;

        case AUP_TOK_LESS_LESS:     emitByte(P, AUP_OP_SHL); break;
        case AUP_TOK_GREATER_GREATER:
                                    emitByte(P, AUP_OP_SHR); break;

        default:
            return; // Unreachable.                              
    }
}

static void call(Parser *P, bool canAssign)
{
    uint8_t argCount = argumentList(P);
    emitBytes(P, AUP_OP_CALL, argCount);
}

static void dot(Parser *P, bool canAssign)
{
    consume(P, AUP_TOK_IDENTIFIER, "Expect member name.");
    uint8_t name = identifierConstant(P, &P->previous);

    if (canAssign && match(P, AUP_TOK_EQUAL)) {
        expression(P);
        emitBytes(P, AUP_OP_SET, (uint8_t)name);
    }
    else {
        emitBytes(P, AUP_OP_GET, (uint8_t)name);
    }
}

static void index_(Parser *P, bool canAssign)
{
    expression(P);
    consume(P, AUP_TOK_RBRACKET, "Expected closing ']'");

    if (canAssign && match(P, AUP_TOK_EQUAL)) {
        expression(P);
        emitByte(P, AUP_OP_SETI);

        P->hadAssign = true;
    }
    else {
        emitByte(P, AUP_OP_GETI);
    }
}

static void ternary(Parser *P, bool canAssign)
{
    int jmp1 = emitJump(P, AUP_OP_JMPF);
    emitByte(P, AUP_OP_POP);

    expression(P);
    int jmp2 = emitJump(P, AUP_OP_JMP);
    
    consume(P, AUP_TOK_COLON, "Expect ':' after value.");

    patchJump(P, jmp1);
    emitByte(P, AUP_OP_POP);
    expression(P);

    patchJump(P, jmp2);
}

static void literal(Parser *P, bool canAssign)
{
    switch (P->previous.type) {
        case AUP_TOK_FALSE:   emitByte(P, AUP_OP_FALSE); break;
        case AUP_TOK_NIL:     emitByte(P, AUP_OP_NIL); break;
        case AUP_TOK_TRUE:    emitByte(P, AUP_OP_TRUE); break;
        case AUP_TOK_FUNC:     emitBytes(P, AUP_OP_LD, 0); break;
        default:
            return; // Unreachable.                   
    }
}

static void grouping(Parser *P, bool canAssign)
{
    expression(P);
    consume(P, AUP_TOK_RPAREN, "Expect ')' after expression.");
}

static void integer(Parser *P, bool canAssign)
{
    int64_t i = 0;

    switch (P->previous.type) {
        case AUP_TOK_INTEGER:
            i = strtoll(P->previous.start, NULL, 10);
            break;
        case AUP_TOK_HEXADECIMAL:
            i = strtoll(P->previous.start + 2, NULL, 16);
            break;
    }

    if (i <= UINT8_MAX) {
        emitBytes(P, AUP_OP_INT, (uint8_t)i);
    }
    else if (i <= UINT16_MAX) {
        emitByte(P, AUP_OP_INTL);
        emitWord(P, (uint16_t)i);
    }
    else {
        emitConstant(P, AUP_NUM((double)i));
    }
}

static void number(Parser *P, bool canAssign)
{
    double n = strtod(P->previous.start, NULL);
    emitConstant(P, AUP_NUM(n));
}

static void string(Parser *P, bool canAssign)
{
    aupStr *s = aup_copyString(P->vm,
        P->previous.start + 1, P->previous.length - 2);

    emitConstant(P, AUP_OBJ(s));
}

static void map(Parser *P, bool canAssign)
{
    uint8_t count = 0;

    if (!check(P, AUP_TOK_RBRACKET)) {
        do {
            expression(P);
            count++;
        } while (match(P, AUP_TOK_COMMA));
    }

    consume(P, AUP_TOK_RBRACKET, "Expected closing ']'.");
    emitBytes(P, AUP_OP_MAP, count);
}

static void namedVariable(Parser *P, aupTok name, bool canAssign)
{
    uint8_t getOp, setOp;
    int arg = resolveLocal(P, P->compiler, &name);

    if (arg != -1) {
        getOp = AUP_OP_LD;
        setOp = AUP_OP_ST;
    }
    else if ((arg = resolveUpvalue(P, P->compiler, &name)) != -1) {
        getOp = AUP_OP_ULD;
        setOp = AUP_OP_UST;
    }
    else {
        arg = identifierConstant(P, &name);
        getOp = AUP_OP_GLD;
        setOp = AUP_OP_GST;
    }

    if (canAssign && match(P, AUP_TOK_EQUAL)) {
        expression(P);
        emitBytes(P, setOp, (uint8_t)arg);

        P->hadAssign = true;
    }
    else if (canAssign && match(P, AUP_TOK_PLUS_EQUAL)) {
        namedVariable(P, name, false);
        expression(P);
        emitByte(P, AUP_OP_ADD);
        emitBytes(P, setOp, (uint8_t)arg);

        P->hadAssign = true;
    }
    else if (canAssign && match(P, AUP_TOK_MINUS_EQUAL)) {
        namedVariable(P, name, false);
        expression(P);
        emitByte(P, AUP_OP_SUB);
        emitBytes(P, setOp, (uint8_t)arg);

        P->hadAssign = true;
    }
    else if (canAssign && match(P, AUP_TOK_STAR_EQUAL)) {
        namedVariable(P, name, false);
        expression(P);
        emitByte(P, AUP_OP_MUL);
        emitBytes(P, setOp, (uint8_t)arg);

        P->hadAssign = true;
    }
    else if (canAssign && match(P, AUP_TOK_SLASH_EQUAL)) {
        namedVariable(P, name, false);
        expression(P);
        emitByte(P, AUP_OP_DIV);
        emitBytes(P, setOp, (uint8_t)arg);

        P->hadAssign = true;
    }
    else if (canAssign && match(P, AUP_TOK_PERCENT_EQUAL)) {
        namedVariable(P, name, false);
        expression(P);
        emitByte(P, AUP_OP_MOD);
        emitBytes(P, setOp, (uint8_t)arg);

        P->hadAssign = true;
    }
    else {
        emitBytes(P, getOp, (uint8_t)arg);
    }
}

static void variable(Parser *P, bool canAssign)
{
    namedVariable(P, P->previous, canAssign);
}

static void or_(Parser *P, bool canAssign)
{
    int elseJump = emitJump(P, AUP_OP_JMPF);
    int endJump = emitJump(P, AUP_OP_JMP);

    patchJump(P, elseJump);
    emitByte(P, AUP_OP_POP);

    parsePrecedence(P, PREC_OR);
    patchJump(P, endJump);
}

static void unary(Parser *P, bool canAssign)
{
    aupTokType operatorType = P->previous.type;

    // Compile the operand.                        
    parsePrecedence(P, PREC_UNARY);

    // Emit the operator instruction.              
    switch (operatorType) {
        case AUP_TOK_NOT:
        case AUP_TOK_BANG:      emitByte(P, AUP_OP_NOT); break;
        case AUP_TOK_MINUS:     emitByte(P, AUP_OP_NEG); break;
        case AUP_TOK_TILDE:     emitByte(P, AUP_OP_BNOT); break;
        default:
            return; // Unreachable.                    
    }
}

static ParseRule rules[AUP_TOKENCOUNT] = {
    [AUP_TOK_LPAREN]        = { grouping, call,    PREC_CALL },
    [AUP_TOK_RPAREN]        = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_LBRACKET]      = { map,      index_,  PREC_CALL },
    [AUP_TOK_RBRACKET]      = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_LBRACE]        = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_RBRACE]        = { NULL,     NULL,    PREC_NONE },

    [AUP_TOK_ARROW]         = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_COMMA]         = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_DOT]           = { NULL,     dot,     PREC_CALL },

    [AUP_TOK_COLON]         = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_QMARK]         = { NULL,     ternary, PREC_TERNARY },

    [AUP_TOK_MINUS]         = { unary,    binary,  PREC_TERM },
    [AUP_TOK_PLUS]          = { NULL,     binary,  PREC_TERM },
    [AUP_TOK_SEMICOLON]     = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_SLASH]         = { NULL,     binary,  PREC_FACTOR },
    [AUP_TOK_STAR]          = { NULL,     binary,  PREC_FACTOR },
    [AUP_TOK_PERCENT]       = { NULL,     binary,  PREC_FACTOR },

    [AUP_TOK_AMPERSAND]     = { NULL,     binary,  PREC_BAND },
    [AUP_TOK_VBAR]          = { NULL,     binary,  PREC_BOR },
    [AUP_TOK_TILDE]         = { unary,    NULL,    PREC_NONE },
    [AUP_TOK_CARET]         = { NULL,     binary,  PREC_BXOR },

    [AUP_TOK_LESS_LESS]     = { NULL,     binary,  PREC_SHIFT },
    [AUP_TOK_GREATER_GREATER]
                            = { NULL,     binary,  PREC_SHIFT },

    [AUP_TOK_BANG]          = { unary,    NULL,    PREC_NONE },
    [AUP_TOK_BANG_EQUAL]    = { NULL,     binary,  PREC_EQUALITY },
    [AUP_TOK_EQUAL]         = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_EQUAL_EQUAL]   = { NULL,     binary,  PREC_EQUALITY },
    [AUP_TOK_GREATER]       = { NULL,     binary,  PREC_COMPARISON },
    [AUP_TOK_GREATER_EQUAL] = { NULL,     binary,  PREC_COMPARISON },
    [AUP_TOK_LESS]          = { NULL,     binary,  PREC_COMPARISON },
    [AUP_TOK_LESS_EQUAL]    = { NULL,     binary,  PREC_COMPARISON },

    [AUP_TOK_IDENTIFIER]    = { variable, NULL,    PREC_NONE },
    [AUP_TOK_STRING]        = { string,   NULL,    PREC_NONE },
    [AUP_TOK_NUMBER]        = { number,   NULL,    PREC_NONE },
    [AUP_TOK_INTEGER]       = { integer,  NULL,    PREC_NONE },
    [AUP_TOK_HEXADECIMAL]   = { integer,  NULL,    PREC_NONE },

    [AUP_TOK_AND]           = { NULL,     and_,    PREC_AND },
    [AUP_TOK_BREAK]         = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_CLASS]         = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_CONTINUE]      = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_DO]            = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_ELSE]          = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_ELSEIF]        = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_END]           = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_FALSE]         = { literal,  NULL,    PREC_NONE },
    [AUP_TOK_FOR]           = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_FUNC]          = { literal,  NULL,    PREC_NONE },
    [AUP_TOK_IF]            = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_LOOP]          = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_MATCH]         = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_NIL]           = { literal,  NULL,    PREC_NONE },
    [AUP_TOK_NOT]           = { unary,    NULL,    PREC_NONE },
    [AUP_TOK_OR]            = { NULL,     or_,     PREC_OR },
    [AUP_TOK_PRINT]         = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_RETURN]        = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_SUPER]         = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_THEN]          = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_THIS]          = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_TRUE]          = { literal,  NULL,    PREC_NONE },
    [AUP_TOK_VAR]           = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_WHILE]         = { NULL,     NULL,    PREC_NONE },

    [AUP_TOK_ERROR]         = { NULL,     NULL,    PREC_NONE },
    [AUP_TOK_EOF]           = { NULL,     NULL,    PREC_NONE }          
};

static void parsePrecedence(Parser *P, Precedence precedence)
{
    advance(P);
    ParseFn prefixRule = getRule(P->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(P, "Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(P, canAssign);
    P->subExprs++;

    while (precedence <= getRule(P->current.type)->precedence) {
        if (P->current.line > P->previous.line) break;
        if (check(P, AUP_TOK_LPAREN)) P->hadCall = true;
        advance(P);
        ParseFn infixRule = getRule(P->previous.type)->infix;
        infixRule(P, canAssign);
    }

    if (canAssign && match(P, AUP_TOK_EQUAL)) {
        error(P, "Invalid assignment target.");
    }
}

static ParseRule *getRule(aupTokType type)
{
    return &rules[type];
}

static void expression(Parser *P)
{
    parsePrecedence(P, PREC_ASSIGNMENT);
}

static void block(Parser *P, aupTokType closing)
{
    while (!check(P, closing) && !check(P, AUP_TOK_EOF)) {
        declaration(P);
    }

    consume(P, closing, "Expect '%s' after block.",
        closing == AUP_TOK_RBRACE ? "}" : "end");
}

static void function(Parser *P, FunType type)
{
    Compiler compiler;
    initCompiler(P, &compiler, type);
    beginScope(P);

    // Compile the parameter list.                                
    consume(P, AUP_TOK_LPAREN, "Expect '(' after function name.");
    if (!check(P, AUP_TOK_RPAREN)) {
        do {
            uint8_t paramConstant = parseVariable(P, "Expect parameter name.");
            defineVariable(P, paramConstant);

            int arity = ++P->compiler->function->arity;
            if (arity > MAX_ARGS) {
                errorAtCurrent(P, "Cannot have more than %d parameters.", MAX_ARGS);
            }
        } while (match(P, AUP_TOK_COMMA));
    }
    consume(P, AUP_TOK_RPAREN, "Expect ')' after parameters.");

    // The body.                     
    if (match(P, AUP_TOK_EQUAL) || match(P, AUP_TOK_ARROW)) {
        // Single expression
        expression(P);
        emitByte(P, AUP_OP_RET);
    }
    else {
        // Block statement.
        aupTokType closing = match(P, AUP_TOK_LBRACE) ?
            AUP_TOK_RBRACE : AUP_TOK_END;

        block(P, closing);
    }

    // Create the function object.                                
    aupFun *function = endCompiler(P);
    uint8_t constant = makeConstant(P, AUP_OBJ(function));

    if (function->upvalueCount > 0) {
        emitBytes(P, AUP_OP_CLOSURE, constant);
        for (int i = 0; i < function->upvalueCount; i++) {
            emitByte(P, compiler.upvalues[i].isLocal ? 1 : 0);
            emitByte(P, compiler.upvalues[i].index);
        }
    }

    emitSmart(P, AUP_OP_CONST, constant);
}

static void funDeclaration(Parser *P)
{
    uint8_t global = parseVariable(P, "Expect function name.");
    markInitialized(P);
    function(P, TYPE_FUNCTION);
    defineVariable(P, global);
}

static void varDeclaration(Parser *P)
{
    uint8_t global = parseVariable(P, "Expect variable name.");

    if (match(P, AUP_TOK_EQUAL)) {
        expression(P);
    }
    else {
        emitByte(P, AUP_OP_NIL);
    }

    defineVariable(P, global);
    match(P, AUP_TOK_SEMICOLON);
}

static void expressionStatement(Parser *P)
{
    P->hadCall = false;
    P->hadAssign = false;
    P->subExprs = 0;

    expression(P);
    emitByte(P, AUP_OP_POP);

    if ((P->subExprs <= 1 && !P->hadCall) ||
        (P->subExprs > 1 && !P->hadCall && !P->hadAssign)) {
        error(P, "Unexpected expression syntax.");
        return;
    }
}

static void ifStatement(Parser *P)
{
    Compiler *current = P->compiler;

    expression(P);

    int thenJump = emitJump(P, AUP_OP_JMPF);
    emitByte(P, AUP_OP_POP);

    bool useThen = !check(P, AUP_TOK_LBRACE);
    if (useThen) consume(P, AUP_TOK_THEN, "Expect 'then' after condition.");

    statement(P);

    int elseJump = emitJump(P, AUP_OP_JMP);

    patchJump(P, thenJump);
    emitByte(P, AUP_OP_POP);

    if (match(P, AUP_TOK_ELSE)) {
        if (match(P, AUP_TOK_IF)) {
            goto _elseif;
        }
        else {
            match(P, AUP_TOK_THEN);
            statement(P);
            current->ifNeedEnd = true;
        }
    }
    else if (match(P, AUP_TOK_ELSEIF)) {
    _elseif:
        current->ifNeedEnd = true;
        ifStatement(P);
        current->ifNeedEnd = false;
    }

    if (useThen && current->ifNeedEnd) {
        consume(P, AUP_TOK_END, "Expect 'end' to close 'if' statement.");
    }

    patchJump(P, elseJump);
}

static void loopStatetment(Parser *P)
{
    // Init loop.
    Loop loop = { 0 };
    Compiler *current = P->compiler;

    current->loopDepth++;
    current->currentLoop = &loop;

    // Get start point.
    loop.start = currentChunk(P)->count;

    // Condition.
    if (check(P, AUP_TOK_LBRACE) || check(P, AUP_TOK_DO)) {
        // Infinite loop.
        emitByte(P, AUP_OP_TRUE);       
    }
    else {
        expression(P);
    }

    int jmpOut = emitJump(P, AUP_OP_JMPF);

    emitByte(P, AUP_OP_POP);
    statement(P);

    emitLoop(P, loop.start);

    patchJump(P, jmpOut);
    emitByte(P, AUP_OP_POP);

    // Patch all breaks.
    for (int i = 0; i < loop.breakCount; i++)
        patchJump(P, loop.breaks[i]);

    current->loopDepth--;
    current->currentLoop = NULL;
}

static void breakStatement(Parser *P)
{
    Compiler *current = P->compiler;
    Loop *loop = current->currentLoop;

    if (current->loopDepth == 0) {
        error(P, "Cannot use 'break' outside of a loop.");
        return;
    }

    int jmpOut = emitJump(P, AUP_OP_JMP);
    loop->breaks[loop->breakCount++] = jmpOut;

    if (loop->breakCount > UINT8_COUNT) {
        error(P, "Too many 'break' statements in this loop.");
        return;
    }
}

static void continueStatement(Parser *P)
{
    Compiler *current = P->compiler;
    Loop *loop = current->currentLoop;

    if (current->loopDepth == 0) {
        error(P, "Cannot use 'continue' outside of a loop.");
        return;
    }

    emitLoop(P, loop->start);
}

static void matchStatement(Parser *P)
{
    expression(P);
    bool hadBrace = match(P, AUP_TOK_LBRACE);

    int caseCount = 0;
    int jmpOuts[MAX_CASES];

    if ((hadBrace && !check(P, AUP_TOK_RBRACE)) ||
        match(P, AUP_TOK_VBAR)) {
        do {
            // Default.
            if (match(P, AUP_TOK_ARROW)) {
                statement(P);
                jmpOuts[caseCount++] = emitJump(P, AUP_OP_JMP);
                continue;
            }

            expression(P);
            int jmpNext = emitJump(P, AUP_OP_JNE);

            consume(P, AUP_TOK_ARROW, "Extect '=>' after expression.");
            statement(P);
         
            jmpOuts[caseCount++] =  emitJump(P, AUP_OP_JMP);
            patchJump(P, jmpNext);

            if (caseCount > MAX_CASES) {
                error(P, "Too many cases in 'match' statement.");
                return;
            }
        } while ((hadBrace && match(P, AUP_TOK_COMMA) && !check(P, AUP_TOK_RBRACE)) ||
            match(P, AUP_TOK_VBAR));
    }

    if (hadBrace) {
        consume(P, AUP_TOK_RBRACE, "Exprect '}' after 'match' statement body.");
        return;
    }

    // Patch all endings.
    for (int i = 0; i < caseCount; i++) patchJump(P, jmpOuts[i]);
    // Pop the input value.
    emitByte(P, AUP_OP_POP);
}

static void printStatement(Parser *P)
{
    int count = 0;

    do {
        count++;
        expression(P);
        
        if (count > MAX_ARGS) {
            error(P, "Too many values in 'print' statement.");
            return;
        }
    } while (match(P, AUP_TOK_COMMA));

    emitBytes(P, AUP_OP_PRINT, count);
}

static void returnStatement(Parser *P)
{
    if (P->compiler->type == TYPE_SCRIPT) {
        error(P, "Cannot return from top-level code.");
    }

    if (match(P, AUP_TOK_SEMICOLON) ||
        check(P, AUP_TOK_RBRACE)) {
        emitReturn(P);
    }
    else {
        expression(P);
        emitByte(P, AUP_OP_RET);
    }
}

static void synchronize(Parser *P)
{
    P->panicMode = false;

    while (P->current.type != AUP_TOK_EOF) {
        if (P->previous.type == AUP_TOK_SEMICOLON) return;

        switch (P->current.type) {
            case AUP_TOK_CLASS:
            case AUP_TOK_FUNC:
            case AUP_TOK_VAR:
            case AUP_TOK_FOR:
            case AUP_TOK_IF:
            case AUP_TOK_LOOP:
            case AUP_TOK_MATCH:
            case AUP_TOK_WHILE:
            case AUP_TOK_PRINT:
            case AUP_TOK_RETURN:
                return;
            default:; // Do nothing.
        }

        advance(P);
    }
}

static void declaration(Parser *P)
{
    if (match(P, AUP_TOK_FUNC)) {
        funDeclaration(P);
    }
    else if (match(P, AUP_TOK_VAR)) {
        varDeclaration(P);
    }
    else {
        statement(P);
    }

    if (P->panicMode) synchronize(P);
}

static void statement(Parser *P)
{
    if (match(P, AUP_TOK_PRINT)) {
        printStatement(P);
    }
    else if (match(P, AUP_TOK_IF)) {
        P->compiler->ifNeedEnd = true;
        ifStatement(P);
    }
    else if (match(P, AUP_TOK_LOOP)) {
        loopStatetment(P);
    }
    else if (match(P, AUP_TOK_MATCH)) {
        matchStatement(P);
    }
    else if (match(P, AUP_TOK_RETURN)) {
        returnStatement(P);
    }
    else if (match(P, AUP_TOK_BREAK)) {
        breakStatement(P);
    }
    else if (match(P, AUP_TOK_CONTINUE)) {
        continueStatement(P);
    }
    else if (match(P, AUP_TOK_LBRACE) || match(P, AUP_TOK_DO)) {
        aupTokType closing = (P->previous.type == AUP_TOK_LBRACE) ?
            AUP_TOK_RBRACE : AUP_TOK_END;
        beginScope(P);
        block(P, closing);
        endScope(P);
    }
    else if (checkPrev(P, AUP_TOK_THEN) || checkPrev(P, AUP_TOK_ELSE)) {
        beginScope(P);

        while (!check(P, AUP_TOK_IF) && !check(P, AUP_TOK_ELSE) &&
            !check(P, AUP_TOK_ELSEIF) && !check(P, AUP_TOK_END) && !check(P, AUP_TOK_EOF)) {
            declaration(P);
        }

        P->compiler->ifNeedEnd = check(P, AUP_TOK_END);
        endScope(P);
    }
    else if (match(P, AUP_TOK_SEMICOLON)) {
        // Do nothing.
    }
    else {
        expressionStatement(P);
    }

    match(P, AUP_TOK_SEMICOLON);
}

aupFun *aup_compile(aupVM *vm, aupSrc *source)
{
    aupLexer L;
    Parser P;
    Compiler C;

    vm->compiler = &C;

    P.vm = vm;
    P.source = source;
    P.lexer = &L;
    P.compiler = NULL;
    P.hadError = false;
    P.panicMode = false;

    aup_initLexer(&L, source->buffer);
    initCompiler(&P, &C, TYPE_SCRIPT);
    
    advance(&P);
    while (!match(&P, AUP_TOK_EOF)) {
        declaration(&P);
    }

    aupFun *function = endCompiler(&P);
    return P.hadError ? NULL : function;
}

void aup_markCompilerRoots(aupVM *vm)
{
    Compiler *compiler = vm->compiler;
    while (compiler != NULL) {
        aup_markObject(vm, (aupObj *)compiler->function);
        compiler = compiler->enclosing;
    }
}
