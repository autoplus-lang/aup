#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "code.h"
#include "object.h"

#define MAX_ARGS    32
#define MAX_LOCALS  244
#define MAX_CASES   128

#define REG     int

typedef struct _Compiler Compiler;

struct Parser
{
    aupVM    *vm;
    aupSrc   *source;
    Compiler *compiler;

    aupTok current;
    aupTok previous;
    bool hadError;
    bool panicMode;

    bool hadCall;
    bool hadAssign;
    int  subExprs;
};

static THREAD_LOCAL struct Parser P;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // = += -= *= /= %= |= &= ^=
    PREC_TERNARY,     // ?:
    PREC_OR,          // or ||
    PREC_AND,         // and &&
    PREC_BOR,         // |
    PREC_BXOR,        // ^
    PREC_BAND,		  // &
    PREC_EQUALITY,    // == !=    
    PREC_COMPARISON,  // < > <= >=
    PREC_SHIFT,       // << >>
    PREC_TERM,        // + -      
    PREC_FACTOR,      // * / %
    PREC_EXPONENT,    // **
    PREC_UNARY,       // ! - not
    PREC_CALL,        // . () ?.
    PREC_PRIMARY
} Precedence;

typedef REG (* PrefixFn)(REG dest, bool canAssign);
typedef REG (* InfixFn)(REG dest, REG left, bool canAssign);
#define PARSE_PREFIX(n)  REG n(REG dest, bool canAssign)
#define PARSE_INFIX(n)   REG n(REG dest, REG left, bool canAssign)

typedef struct {
    PrefixFn prefix;
    InfixFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    aupTok name;
    int depth;
} Local;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} TFunc;

struct _Compiler {
    Compiler *enclosing;
    aupFun *function;
    TFunc type;

    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
    int localTotal;

    REG regCount;
};

#define VM          P.vm
#define COMPILER    P.compiler
#define PREVIOUS    P.previous
#define CURRENT     P.current
#define CHUNK       getChunk()

#define REG_COUNT   COMPILER->regCount
#define PUSH()      (REG_COUNT++)
#define POP()       (--REG_COUNT)
#define POP_N(n)    (REG_COUNT -= (n))
#define PEEK(i)     (REG_COUNT - 1 - (i))

#define IS_K(r)     ((r) > UINT8_MAX)
#define IS_NEWLINE() \
    (CURRENT.line > PREVIOUS.line)

static aupChunk *getChunk()
{
    return &P.compiler->function->chunk;
}

static void errorAt(aupTok *token, const char *fmt, ...)
{
    if (P.panicMode) return;
    P.panicMode = true;

    fprintf(stderr, "[%d:%d] Error", token->line, token->column);

    if (token->type == AUP_TOK_EOF) {
        fprintf(stderr, " at end");
    }
    else if (token->type == AUP_TOK_ERROR) {
        // Nothing.                                                
    }
    else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, ": ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    P.hadError = true;
}

#define error(fmt, ...) \
    errorAt(&PREVIOUS, fmt, ##__VA_ARGS__)

#define errorAtCurrent(fmt, ...) \
    errorAt(&P.current, fmt, ##__VA_ARGS__)

#define consume(ttok, fmt, ...) \
    do { \
		if (P.current.type == (ttok)) { \
			advance(); \
			break; \
		} \
		errorAtCurrent(fmt, ##__VA_ARGS__); \
	} while (0)

static bool preparse(aupTok token)
{
    // TODO
    return false;
}

static void advance()
{
    PREVIOUS = CURRENT;

    for (;;) {
        aupTok token = aup_scanToken();

        if (token.type == AUP_TOK_ERROR) {
            CURRENT = token;
            errorAtCurrent(CURRENT.start);
        }
        else {
            CURRENT = preparse(token) ?
                aup_scanToken() : token;
            break;
        }
    }
}

static bool check(aupTTok type)
{
    return P.current.type == type;
}

static bool match(aupTTok type)
{
    if (!check(type)) return false;
    advance();
    return true;
}

static void emit(uint32_t instruction)
{
    aup_emitChunk(getChunk(),
        instruction, PREVIOUS.line, PREVIOUS.column);
}

static int emitJump(aupOp jmpOp, REG src)
{
    if (jmpOp == AUP_OP_JMP)
        emit(AUP_OpAxx(jmpOp, 0));
    else
        emit(AUP_OpAxxCx(jmpOp, 0, src));
    
    return getChunk()->count - 1;
}

static void patchJump(int offset)
{
    // -1, backtrack after [ip++].
    aupChunk *chunk = getChunk();
    int jump = chunk->count - offset - 1;

    if (jump > INT16_MAX) {
        error("Too much code to jump over.");
    }

    uint32_t i = chunk->code[offset];
    uint8_t op = AUP_GetOp(i);
    REG RK = AUP_GetCx(i);

    // Patch the hole.
    i = AUP_OpAxxCx(op, jump, RK);
    chunk->code[offset] = i;
}

static void emitReturn(REG src)
{
    aupChunk *chunk = getChunk();

    if (chunk->count == 0 || AUP_GetOp(chunk->code[chunk->count - 1]) != AUP_OP_RET) {
        if (src == -1) {
            emit(AUP_OpA(AUP_OP_RET, false));
        }
        else {
            emit(AUP_OpABx(AUP_OP_RET, true, src));
        }
    }
}

static uint8_t makeConstant(aupVal value)
{
    int constant = aup_addConstant(getChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static REG emitConstant(aupVal value)
{
    return makeConstant(value) + UINT8_COUNT;
}

static void initCompiler(Compiler *compiler, TFunc type)
{
    compiler->enclosing = COMPILER;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->localTotal = 0;
    compiler->function = aup_newFunction(VM, P.source);

    COMPILER = compiler;
    REG_COUNT = 0;

    if (type != TYPE_SCRIPT) {
        COMPILER->function->name = aup_copyString(VM,
            PREVIOUS.start, PREVIOUS.length);
    }

    Local *local = &COMPILER->locals[COMPILER->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
    COMPILER->localTotal++;

    PUSH();
}

static aupFun *endCompiler()
{
    emitReturn(-1);
    aupFun *function = COMPILER->function;
    function->locals = COMPILER->localTotal;

    if (!P.hadError) {
        aup_dasmChunk(CHUNK,
            function->name != NULL ? function->name->chars : "<script>");
    }
   
    COMPILER = COMPILER->enclosing;
    return function;
}

static void beginScope()
{
    COMPILER->scopeDepth++;
}

static void endScope()
{
    COMPILER->scopeDepth--;

    while (COMPILER->localCount > 0 &&
        COMPILER->locals[COMPILER->localCount - 1].depth > COMPILER->scopeDepth) {
        //emitByte(OP_POP);
        COMPILER->localCount--;
    }
}

static void stmt();
static void decl();
static REG  expr(REG dest);
static REG  exprEx(REG dest);
static REG  exprPrec(REG dest, Precedence prec);
static REG  parsePrec(REG dest, Precedence prec);
static ParseRule *getRule(aupTTok type);

static uint8_t identifierConstant(aupTok *name)
{
    aupStr *identifier = aup_copyString(VM,
        name->start, name->length);
    return makeConstant(AUP_VObj(identifier));
}

static bool identifiersEqual(aupTok *a, aupTok *b)
{
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, aupTok *name)
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

static void addLocal(aupTok name)
{
    if (COMPILER->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    Local *local = &COMPILER->locals[COMPILER->localCount++];
    local->name = name;
    local->depth = -1;

    if (COMPILER->localCount > COMPILER->localTotal)
        COMPILER->localTotal++;
}

static void declareVariable()
{
    // Global variables are implicitly declared.
    if (COMPILER->scopeDepth == 0) return;

    aupTok *name = &PREVIOUS;
    for (int i = COMPILER->localCount - 1; i >= 0; i--) {
        Local *local = &COMPILER->locals[i];
        if (local->depth != -1 && local->depth < COMPILER->scopeDepth) {
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
    consume(AUP_TOK_IDENTIFIER, errorMessage);

    declareVariable();
    if (COMPILER->scopeDepth > 0) return 0;

    return identifierConstant(&PREVIOUS);
}

static void markInitialized()
{
    if (COMPILER->scopeDepth == 0) return;

    COMPILER->locals[COMPILER->localCount - 1].depth =
        COMPILER->scopeDepth;
}

static void defineVariable(REG global, REG src)
{
    if (COMPILER->scopeDepth > 0) {
        markInitialized();
        return;
    }

    if (src == -1)
        emit(AUP_OpAsC(AUP_OP_GST, global, true));
    else
        emit(AUP_OpABx(AUP_OP_GST, global, src));
}

static uint8_t argumentList()
{
    uint8_t argCount = 0;
    if (!check(AUP_TOK_RPAREN)) {
        do {
            exprEx(-1);
            if (++argCount > MAX_ARGS) {
                error("Cannot have more than %d arguments.", MAX_ARGS);
            }
        } while (match(AUP_TOK_COMMA));
    }

    consume(AUP_TOK_RPAREN, "Expect ')' after arguments.");
    return argCount;
}

static PARSE_INFIX(and_)
{
    int endJump = emitJump(AUP_OP_JMPF, left);

    dest = exprPrec(dest, PREC_AND);
    patchJump(endJump);

    return dest;
}

static PARSE_INFIX(or__)
{
    int elseJump = emitJump(AUP_OP_JMPF, left);
    int endJump = emitJump(AUP_OP_JMP, -1);

    patchJump(elseJump);

    exprPrec(left, PREC_OR);
    patchJump(endJump);

    return left;
}

static PARSE_INFIX(binary)
{
    // Remember the operator.
    aupTTok operatorType = PREVIOUS.type;

    // Compile the right operand.                            
    ParseRule *rule = getRule(operatorType);
    REG right = parsePrec(-1, (Precedence)(rule->precedence + 1));
    POP();

    // Emit the operator instruction.
    switch (operatorType) {
        case AUP_TOK_LESS:
            emit(AUP_OpABxCx(AUP_OP_LT, dest, left, right));
            break;
        case AUP_TOK_LESS_EQUAL:
            emit(AUP_OpABxCx(AUP_OP_LE, dest, left, right));
            break;
        case AUP_TOK_EQUAL_EQUAL:
            emit(AUP_OpABxCx(AUP_OP_EQ, dest, left, right));
            break;
        case AUP_TOK_BANG_EQUAL:
            emit(AUP_OpABxCx(AUP_OP_EQ, dest, left, right));
            emit(AUP_OpABx(AUP_OP_NOT, dest, dest));
            break;
        case AUP_TOK_GREATER:
            emit(AUP_OpABxCx(AUP_OP_GT, dest, left, right));
            break;
        case AUP_TOK_GREATER_EQUAL:
            emit(AUP_OpABxCx(AUP_OP_GE, dest, left, right));
            break;
        case AUP_TOK_PLUS:
            emit(AUP_OpABxCx(AUP_OP_ADD, dest, left, right));
            break;
        case AUP_TOK_MINUS:
            emit(AUP_OpABxCx(AUP_OP_SUB, dest, left, right));
            break;
        case AUP_TOK_STAR:
            emit(AUP_OpABxCx(AUP_OP_MUL, dest, left, right));
            break;
        case AUP_TOK_SLASH:
            emit(AUP_OpABxCx(AUP_OP_DIV, dest, left, right));
            break;
        case AUP_TOK_PERCENT:
            emit(AUP_OpABxCx(AUP_OP_MOD, dest, left, right));
            break;
        case AUP_TOK_STAR_STAR:
            emit(AUP_OpABxCx(AUP_OP_POW, dest, left, right));
            break;
        case AUP_TOK_AMPERSAND:
            emit(AUP_OpABxCx(AUP_OP_BAND, dest, left, right));
            break;
        case AUP_TOK_VBAR:
            emit(AUP_OpABxCx(AUP_OP_BOR, dest, left, right));
            break;
        case AUP_TOK_CARET:
            emit(AUP_OpABxCx(AUP_OP_BXOR, dest, left, right));
            break;
        case AUP_TOK_LESS_LESS:
            emit(AUP_OpABxCx(AUP_OP_SHL, dest, left, right));
            break;
        case AUP_TOK_GREATER_GREATER:
            emit(AUP_OpABxCx(AUP_OP_SHR, dest, left, right));
            break;
    }

    return dest;
}

static PARSE_INFIX(call)
{
    int argc = argumentList();

    emit(AUP_OpAB(AUP_OP_CALL, dest, argc));
    POP_N(argc);

    return dest;
}

static PARSE_INFIX(dot_)
{
    // TODO
    return dest;
}

static PARSE_PREFIX(member)
{
    bool isDtor = match(AUP_TOK_TILDE);

    if (match(AUP_TOK_LPAREN)) {

    }
    else {

    }

    // TODO
    return dest;
}

static PARSE_INFIX(nullCond)
{
    error("This operator is not implemented!");
    return dest;
}

static PARSE_INFIX(ternary)
{
    int jmp1 = emitJump(AUP_OP_JMPF, left);

    exprEx(left);
    int jmp2 = emitJump(AUP_OP_JMP, -1);

    consume(AUP_TOK_COLON, "Expect ':' after value.");

    patchJump(jmp1);
    exprEx(left);

    patchJump(jmp2);

    return dest;
}

static PARSE_PREFIX(literal)
{
    switch (PREVIOUS.type) {
        case AUP_TOK_KW_NIL:
            emit(AUP_OpA(AUP_OP_NIL, dest));
            break;
        case AUP_TOK_KW_TRUE:
            emit(AUP_OpAsB(AUP_OP_BOOL, dest, true));
            break;
        case AUP_TOK_KW_FALSE:
            emit(AUP_OpAsB(AUP_OP_BOOL, dest, false));
            break;
        case AUP_TOK_KW_FUNC:
            emit(AUP_OpAB(AUP_OP_MOV, dest, 0));
            break;
    }

    return dest;
}

static PARSE_PREFIX(grouping)
{
    dest = expr(dest);
    consume(AUP_TOK_RPAREN, "Expect ')' after expression.");

    return dest;
}

static PARSE_PREFIX(number)
{
    double value = strtod(PREVIOUS.start, NULL);
    return emitConstant(AUP_VNum(value));
}

static PARSE_PREFIX(integer)
{
    int64_t value = 0;

    switch (PREVIOUS.type) {
        case AUP_TOK_INTEGER:
            value = atoll(PREVIOUS.start);
            break;
        case AUP_TOK_HEXADECIMAL:
            value = strtoll(PREVIOUS.start + 2, NULL, 16);
            break;
    }

    return emitConstant(AUP_VNum(value));
}

static PARSE_PREFIX(string)
{
    int length = PREVIOUS.length - 2;
    const char *start = PREVIOUS.start + 1;

    aupStr *string = aup_copyString(VM, start, length);
    return emitConstant(AUP_VObj(string));
}

static REG namedVariable(aupTok name, REG dest, bool canAssign)
{
    bool isLocal = false;
    uint8_t loadOp, storeOp;
    int arg = resolveLocal(COMPILER, &name);

    if (arg != -1) {
        isLocal = true;
        loadOp = AUP_OP_LD;
        storeOp = AUP_OP_LD;
    }
    else {
        arg = identifierConstant(&name);
        loadOp = AUP_OP_GLD;
        storeOp = AUP_OP_GST;
    }

    if (canAssign && match(AUP_TOK_EQUAL)) {
        REG src = exprEx(dest);
        emit(AUP_OpABx(storeOp, arg, src));

        dest = src;
        P.hadAssign = true;
    }
    else {
        if (isLocal) return arg;
        emit(AUP_OpABx(loadOp, dest, arg));
    }

    return dest;
}

static PARSE_PREFIX(variable)
{
    return namedVariable(PREVIOUS, dest, canAssign);
}

static PARSE_PREFIX(unary)
{
    aupTTok operatorType = PREVIOUS.type;

    // Compile the operand.                        
    REG right = parsePrec(dest, PREC_UNARY);

    // Emit the operator instruction.              
    switch (operatorType) {
        case AUP_TOK_TILDE:
            emit(AUP_OpABx(AUP_OP_BNOT, dest, right));
            break;
        case AUP_TOK_KW_NOT:
        case AUP_TOK_BANG:
            emit(AUP_OpABx(AUP_OP_NOT, dest, right));
            break;
        case AUP_TOK_MINUS:
            emit(AUP_OpABx(AUP_OP_NEG, dest, right));
            break;
    }

    return dest;
}

static ParseRule rules[] = {
    // Characters
    [AUP_TOK_LPAREN         ] = { grouping, call,       PREC_CALL       },
    [AUP_TOK_RPAREN         ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_LBRACE         ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_RBRACE         ] = { NULL,     NULL,       PREC_NONE       },
    //
    [AUP_TOK_COMMA          ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_DOT            ] = { NULL,     dot_,       PREC_CALL       },
    [AUP_TOK_COLON          ] = { member,   NULL,       PREC_NONE       },
    [AUP_TOK_SEMICOLON      ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_QMARK          ] = { NULL,     ternary,    PREC_TERNARY    },
    //
    [AUP_TOK_MINUS          ] = { unary,    binary,     PREC_TERM       },
    [AUP_TOK_PLUS           ] = { NULL,     binary,     PREC_TERM       },
    [AUP_TOK_SLASH          ] = { NULL,     binary,     PREC_FACTOR     },
    [AUP_TOK_STAR           ] = { NULL,     binary,     PREC_FACTOR     },
    [AUP_TOK_PERCENT        ] = { NULL,     binary,     PREC_FACTOR     },
    //
    [AUP_TOK_AMPERSAND      ] = { NULL,     binary,     PREC_BAND       },
    [AUP_TOK_VBAR           ] = { NULL,     binary,     PREC_BOR        },
    [AUP_TOK_TILDE          ] = { unary,    NULL,       PREC_NONE       },
    [AUP_TOK_CARET          ] = { NULL,     binary,     PREC_BXOR       },
    [AUP_TOK_LESS_LESS      ] = { NULL,     binary,     PREC_SHIFT      },
    [AUP_TOK_GREATER_GREATER] = { NULL,     binary,     PREC_SHIFT      },
    //
    [AUP_TOK_BANG           ] = { unary,    NULL,       PREC_NONE       },
    [AUP_TOK_BANG_EQUAL     ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_BANG_EQUAL     ] = { NULL,     binary,     PREC_EQUALITY   },
    [AUP_TOK_EQUAL_EQUAL    ] = { NULL,     binary,     PREC_EQUALITY   },
    [AUP_TOK_GREATER        ] = { NULL,     binary,     PREC_COMPARISON },
    [AUP_TOK_GREATER_EQUAL  ] = { NULL,     binary,     PREC_COMPARISON },
    [AUP_TOK_LESS           ] = { NULL,     binary,     PREC_COMPARISON },
    [AUP_TOK_LESS_EQUAL     ] = { NULL,     binary,     PREC_COMPARISON },
    //
    [AUP_TOK_AMPERSAND_EQUAL] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_VBAR_EQUAL     ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_CARET_EQUAL    ] = { NULL,     NULL,       PREC_NONE       },
    //
    [AUP_TOK_PLUS_EQUAL     ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_MINUS_EQUAL    ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_STAR_EQUAL     ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_SLASH_EQUAL    ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_PERCENT_EQUAL  ] = { NULL,     NULL,       PREC_NONE       },
    //
    [AUP_TOK_STAR_STAR      ] = { NULL,     binary,     PREC_EXPONENT   },
    [AUP_TOK_QMARK_DOT      ] = { NULL,     nullCond,   PREC_CALL       },
    [AUP_TOK_ARROW          ] = { NULL,     NULL,       PREC_NONE       },
    // Literals
    [AUP_TOK_IDENTIFIER     ] = { variable, NULL,       PREC_NONE       },
    [AUP_TOK_STRING         ] = { string,   NULL,       PREC_NONE       },
    [AUP_TOK_NUMBER         ] = { number,   NULL,       PREC_NONE       },
    [AUP_TOK_INTEGER        ] = { integer,  NULL,       PREC_NONE       },
    [AUP_TOK_HEXADECIMAL    ] = { integer,  NULL,       PREC_NONE       },
    // Keywords
    [AUP_TOK_KW_AND         ] = { NULL,     and_,       PREC_AND        },
    [AUP_TOK_KW_BREAK       ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_CASE        ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_CLASS       ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_DO          ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_ELSE        ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_FALSE       ] = { literal,  NULL,       PREC_NONE       },
    [AUP_TOK_KW_FOR         ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_FUNC        ] = { literal,  NULL,       PREC_NONE       },
    [AUP_TOK_KW_IF          ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_MATCH       ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_NIL         ] = { literal,  NULL,       PREC_NONE       },
    [AUP_TOK_KW_NOT         ] = { unary,    NULL,       PREC_NONE       },
    [AUP_TOK_KW_OR          ] = { NULL,     or__,       PREC_OR         },
    [AUP_TOK_KW_PUTS        ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_RETURN      ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_SUPER       ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_SWITCH      ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_THEN        ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_THIS        ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_TRUE        ] = { literal,  NULL,       PREC_NONE       },
    [AUP_TOK_KW_VAR         ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_KW_WHILE       ] = { NULL,     NULL,       PREC_NONE       },
    //
    [AUP_TOK_ERROR          ] = { NULL,     NULL,       PREC_NONE       },
    [AUP_TOK_EOF            ] = { NULL,     NULL,       PREC_NONE       },   
};

static REG parsePrec(REG dest, Precedence prec)
{
    if (dest < 0) dest = PUSH();

    advance();
    PrefixFn prefixRule = getRule(PREVIOUS.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return -1;
    }

    bool canAssign = prec <= PREC_ASSIGNMENT;
    REG src = prefixRule(dest, canAssign);
    P.subExprs++;

    while (prec <= getRule(CURRENT.type)->precedence && !IS_NEWLINE()) {
        if (check(AUP_TOK_LPAREN)) {
            P.hadCall = true;
        }
        advance();
        InfixFn infixRule = getRule(PREVIOUS.type)->infix;
        src = infixRule(dest, src, canAssign);
        //P.subExprs++;
    }

    if (canAssign && match(AUP_TOK_EQUAL)) {
        error("Invalid assignment target.");
        return -1;
    }

    return src;
}

static ParseRule *getRule(aupTTok type)
{
    return &rules[type];
}

static void block()
{
    while (!check(AUP_TOK_RBRACE) && !check(AUP_TOK_EOF)) {
        decl();
    }

    consume(AUP_TOK_RBRACE, "Expect '}' after block.");
}

static REG exprPrec(REG dest, Precedence prec)
{
    P.hadCall = false;
    P.hadAssign = false;
    P.subExprs = 0;

    dest = parsePrec(dest, prec);

    if (P.subExprs <= 1) {
        REG src = dest;
        REG dest = PEEK(0);
        emit(AUP_OpABx(AUP_OP_LD, dest, src));
    }

    return dest;
}

static REG exprEx(REG dest)
{
    return exprPrec(dest, PREC_ASSIGNMENT);
}

static REG expr(REG dest)
{
    return parsePrec(dest, PREC_ASSIGNMENT);
}

static REG func(TFunc type)
{
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    // Compile the parameter list.                                
    consume(AUP_TOK_LPAREN, "Expect '(' after function name.");
    if (!check(AUP_TOK_RPAREN)) {
        do {
            if (++COMPILER->function->arity > 255) {
                errorAtCurrent("Cannot have more than 255 parameters.");
            }
            uint8_t paramConstant = parseVariable("Expect parameter name.");
            defineVariable(paramConstant, -1);
            PUSH();
        } while (match(AUP_TOK_COMMA));
    }
    consume(AUP_TOK_RPAREN, "Expect ')' after parameters.");

    // The body.                                                  
    consume(AUP_TOK_LBRACE, "Expect '{' before function body.");
    block();

    // Create the function object.                                
    aupFun *function = endCompiler();
    return emitConstant(AUP_VObj(function));
}

static void funcDecl()
{
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    REG src = func(TYPE_FUNCTION);

    defineVariable(global, src);
}

static void varDecl()
{
    uint8_t global = parseVariable("Expect variable name.");
    REG src = -1;   // nil

    if (match(AUP_TOK_EQUAL)) {
        src = exprEx(-1);
    }

    defineVariable(global, src);
}

static void exprStmt()
{
    exprEx(-1);

    //if ((P.subExprs <= 1 && !P.hadCall) ||
    //    (P.subExprs > 1 && !P.hadCall && !P.hadAssign)) {
    //    error("Unexpected expression syntax.");
    //}

    if (!P.hadCall &&
        (P.subExprs <= 1 || !P.hadAssign)) {
        error("Unexpected expression syntax.");
    }
}

static void ifStmt()
{
    REG src = expr(-1);

    if (!match(AUP_TOK_KW_THEN) && !check(AUP_TOK_LBRACE)) {
        error("Expect 'then' or a block after condition.");
        return;
    }

    int thenJump = emitJump(AUP_OP_JMPF, src);
    POP();
    stmt();

    if (match(AUP_TOK_KW_ELSE)) {
        int elseJump = emitJump(AUP_OP_JMP, -1);
        patchJump(thenJump);
        stmt();
        patchJump(elseJump);
    }
    else {
        patchJump(thenJump);
    }
}

static void putsStmt()
{
    int count = 0;
    REG src = PEEK(-1);

    do {
        exprEx(-1);
        count++;
    } while (match(AUP_TOK_COMMA));

    emit(AUP_OpAB(AUP_OP_PRI, src, count));
    POP_N(count);
}

static void returnStmt()
{
    if (COMPILER->type == TYPE_SCRIPT) {
        error("Cannot return from top-level code.");
        return;
    }

    if (check(AUP_TOK_RBRACE) || IS_NEWLINE()) {
        emitReturn(-1);
    }
    else {
        REG src = expr(-1);
        emitReturn(src);
    }
}

static void matchStmt()
{
    int caseCount = 0;
    int jmpOuts[MAX_CASES];
    REG src = PEEK(-1); exprEx(-1);

    while (match(AUP_TOK_VBAR)) {
        // The default
        if (match(AUP_TOK_ARROW)) {
            stmt();
            jmpOuts[caseCount++] = emitJump(AUP_OP_JMP, -1);
        }
        else {
            exprEx(-1); POP();
            int jmpNext = emitJump(AUP_OP_JNE, src + 1);            

            consume(AUP_TOK_ARROW, "Extect '=>' after expression.");
            stmt();

            jmpOuts[caseCount++] = emitJump(AUP_OP_JMP, -1);
            patchJump(jmpNext);
        }

        if (caseCount > MAX_CASES) {
            error("Too many cases in 'match' statement.");
            return;
        }
    }
    
    POP();
    for (int i = 0; i < caseCount; i++) patchJump(jmpOuts[i]);
}

static void synchronize()
{
    P.panicMode = false;

    while (CURRENT.type != AUP_TOK_EOF) {
        //if (PREVIOUS.type == AUP_TOK_SEMICOLON) return;
        if (IS_NEWLINE()) return;

        switch (CURRENT.type) {
            case AUP_TOK_KW_CLASS:
            case AUP_TOK_KW_FUNC:
            case AUP_TOK_KW_VAR:
            case AUP_TOK_KW_FOR:
            case AUP_TOK_KW_IF:
            case AUP_TOK_KW_WHILE:
            case AUP_TOK_KW_PUTS:
            case AUP_TOK_KW_RETURN:
                return;
        }

        advance();
    }
}

static void decl()
{
    if (match(AUP_TOK_KW_VAR)) {
        varDecl();
    }
    else if (match(AUP_TOK_KW_FUNC)) {
        funcDecl();
    }
    else {
        stmt();
    }

    if (P.panicMode) synchronize();
}

static void stmt()
{
    if (match(AUP_TOK_KW_PUTS)) {
        putsStmt();
    }
    else if (match(AUP_TOK_KW_IF)) {
        ifStmt();
    }
    else if (match(AUP_TOK_KW_RETURN)) {
        returnStmt();
    }
    else if (match(AUP_TOK_KW_MATCH)) {
        matchStmt();
    }
    else if (match(AUP_TOK_LBRACE)) {
        beginScope();
        block();
        endScope();
    }
    else {
        exprStmt();
    }
}

aupFun *aup_compile(aupVM *vm, aupSrc *source)
{
    VM = vm;
    COMPILER = NULL;
    P.hadError = false;
    P.panicMode = false;

    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);
    aup_initLexer(source->buffer);
   
    advance();
    while (!match(AUP_TOK_EOF)) {
        decl();
    }

    aupFun *function = endCompiler();
    return P.hadError ? NULL : function;
}
