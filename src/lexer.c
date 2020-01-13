#include <stdio.h>
#include <string.h>

#include "code.h"

void aup_initLexer(aupLexer *L, const char *source)
{
    L->start = source;
    L->current = source;
    L->lineStart = source;

    L->line = 1;
    L->position = 1;
}

static bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c == '_')
        || (c == '$');
}

static bool isDigit(char c)
{
    return (c >= '0')
        && (c <= '9');
}

static bool isHexaDigit(char c)
{
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F');
}

static bool isAtEnd(aupLexer *L)
{
    return *L->current == '\0';
}

static char advance(aupLexer *L)
{
    L->current++;
    L->position++;
    return L->current[-1];
}

static char peek(aupLexer *L)
{
    return *L->current;
}

static char peekNext(aupLexer *L)
{
    if (isAtEnd(L)) return '\0';
    return L->current[1];
}

static void newLine(aupLexer *L)
{
    L->line++;
    L->position = 0;
    L->lineStart = L->current + 1;
}

static bool match(aupLexer *L, char expected)
{
    if (isAtEnd(L)) return false;
    if (*L->current != expected) return false;

    L->current++;
    L->position++;
    return true;
}

static aupTok makeToken(aupLexer *L, aupTokType type)
{
    aupTok token;
    token.type = type;
    token.start = L->start;
    token.length = (int)(L->current - L->start);
    token.line = L->line;
    token.column = L->position - token.length;

    const char *endLine = strchr(L->lineStart, '\n');
    if (endLine == NULL) L->lineLength = (int)strlen(L->lineStart);
    else L->lineLength = endLine - L->lineStart - 1;
    token.lineStart = L->lineStart;
    token.lineLength = L->lineLength;

    return token;
}

static aupTok errorToken(aupLexer *L, const char *message)
{
    aupTok token = makeToken(L, AUP_TOK_ERROR);
    token.start = message;

    return token;
}

static void skipWhitespace(aupLexer *L)
{
    for (;;) {
        char c = peek(L);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(L);
                break;

            case '\n':
                newLine(L);
                advance(L);
                break;

            case '/':
                if (peekNext(L) == '/') {
                    // A comment goes until the end of the line.   
                    while (peek(L) != '\n' && !isAtEnd(L)) advance(L);
                }
                else {
                    return;
                }
                break;

        default:
            return;
        }
    }
}

static aupTokType checkKeyword(aupLexer *L, int start, int length, const char *rest, aupTokType type)
{
    if (L->current - L->start == start + length &&
        memcmp(L->start + start, rest, length) == 0) {
        return type;
    }

    return AUP_TOK_IDENTIFIER;
}

static aupTokType identifierType(aupLexer *L)
{
    switch (L->start[0]) {
        case 'a': return checkKeyword(L, 1, 2, "nd", AUP_TOK_AND);
        case 'c': return checkKeyword(L, 1, 4, "lass", AUP_TOK_CLASS);
        case 'd': return checkKeyword(L, 1, 1, "o", AUP_TOK_DO);
        case 'e': 
            if (L->current - L->start > 1) {
                switch (L->start[1]) {
                    case 'l': return checkKeyword(L, 2, 2, "se", AUP_TOK_ELSE);
                    case 'n': return checkKeyword(L, 2, 1, "d", AUP_TOK_END);
                }
            }
            break;
        case 'f':
            if (L->current - L->start > 1) {
                switch (L->start[1]) {
                    case 'a': return checkKeyword(L, 2, 3, "lse", AUP_TOK_FALSE);
                    case 'o': return checkKeyword(L, 2, 1, "r", AUP_TOK_FOR);
                    case 'u': return checkKeyword(L, 2, 2, "nc", AUP_TOK_FUNC);
                }
            }
            break;
        case 'i': return checkKeyword(L, 1, 1, "f", AUP_TOK_IF);
        case 'l': return checkKeyword(L, 1, 3, "oop", AUP_TOK_LOOP);
        case 'm': return checkKeyword(L, 1, 4, "atch", AUP_TOK_MATCH);
        case 'n':
            if (L->current - L->start > 1) {
                switch (L->start[1]) {
                    case 'i': return checkKeyword(L, 2, 1, "l", AUP_TOK_NIL);
                    case 'o': return checkKeyword(L, 2, 1, "t", AUP_TOK_NOT);
                }
            }
            break;
        case 'o': return checkKeyword(L, 1, 1, "r", AUP_TOK_OR);
        case 'p': return checkKeyword(L, 1, 4, "rint", AUP_TOK_PRINT);
        case 'r': return checkKeyword(L, 1, 5, "eturn", AUP_TOK_RETURN);
        case 's': return checkKeyword(L, 1, 4, "uper", AUP_TOK_SUPER);
        case 't':
            if (L->current - L->start > 1) {
                switch (L->start[1]) {
                    case 'h': 
                        if (L->current - L->start > 2) {
                            switch (L->start[2]) {
                                case 'e': return checkKeyword(L, 3, 1, "n", AUP_TOK_THEN);
                                case 'i': return checkKeyword(L, 3, 1, "s", AUP_TOK_THIS);
                            }
                        }
                        break;                       
                    case 'r': return checkKeyword(L, 2, 2, "ue", AUP_TOK_TRUE);
                }
            }
            break;
        case 'v': return checkKeyword(L, 1, 2, "ar", AUP_TOK_VAR);
        case 'w': return checkKeyword(L, 1, 4, "hile", AUP_TOK_WHILE);
    }

    return AUP_TOK_IDENTIFIER;
}

static aupTok identifier(aupLexer *L)
{
    while (isAlpha(peek(L)) || isDigit(peek(L))) advance(L);

    return makeToken(L, identifierType(L));
}

static aupTok number(aupLexer *L, char start)
{
    if (start == '0' && isAlpha(peek(L))) {
        if ((peek(L) == 'x' || peek(L) == 'X')) {
            advance(L);
            while (isAlpha(peek(L)) || isDigit(peek(L))) {
                if (!isHexaDigit(peek(L))) {
                    return errorToken(L, "Expect hexadecimal digit.");
                }
                advance(L);
            }
            if (L->current - L->start <= 2) {
                return errorToken(L, "Expect hexadecimal digit.");
            }
            return makeToken(L, AUP_TOK_HEXADECIMAL);           
        }
        else {
            return errorToken(L, "Unexpected number format.");
        }
    }

    while (isDigit(peek(L))) advance(L);

    // Look for a fractional part.             
    if (peek(L) == '.' && isDigit(peekNext(L))) {
        // Consume the ".".                      
        advance(L);

        while (isDigit(peek(L))) advance(L);

        return makeToken(L, AUP_TOK_NUMBER);
    }

    return makeToken(L, AUP_TOK_INTEGER);
}

static aupTok string(aupLexer *L, char start)
{
    while (peek(L) != start && !isAtEnd(L)) {
        if (peek(L) == '\n') newLine(L);
        advance(L);
    }

    if (isAtEnd(L)) return errorToken(L, "Unterminated string.");

    // The closing quote.                                    
    advance(L);
    return makeToken(L, AUP_TOK_STRING);
}

aupTok aup_scanToken(aupLexer *L)
{
    skipWhitespace(L);

    L->start = L->current;

    if (isAtEnd(L)) return makeToken(L, AUP_TOK_EOF);

    char c = advance(L);
    if (isAlpha(c)) return identifier(L);
    if (isDigit(c)) return number(L, c);

    switch (c) {
        case '(': return makeToken(L, AUP_TOK_LPAREN);
        case ')': return makeToken(L, AUP_TOK_RPAREN);
        case '[': return makeToken(L, AUP_TOK_LBRACKET);
        case ']': return makeToken(L, AUP_TOK_RBRACKET);
        case '{': return makeToken(L, AUP_TOK_LBRACE);
        case '}': return makeToken(L, AUP_TOK_RBRACE);

        case ';': return makeToken(L, AUP_TOK_SEMICOLON);
        case ',': return makeToken(L, AUP_TOK_COMMA);
        case '.': return makeToken(L, AUP_TOK_DOT);

        case ':': return makeToken(L, AUP_TOK_COLON);
        case '?': return makeToken(L, AUP_TOK_QMARK);

        case '&': return makeToken(L, AUP_TOK_AMPERSAND);
        case '|': return makeToken(L, AUP_TOK_VBAR);
        case '~': return makeToken(L, AUP_TOK_TILDE);
        case '^': return makeToken(L, AUP_TOK_CARET);

        case '+': return makeToken(L, match(L, '=') ? AUP_TOK_PLUS_EQUAL : AUP_TOK_PLUS);
        case '*': return makeToken(L, match(L, '=') ? AUP_TOK_STAR_EQUAL : AUP_TOK_STAR);
        case '/': return makeToken(L, match(L, '=') ? AUP_TOK_SLASH_EQUAL : AUP_TOK_SLASH);
        case '%': return makeToken(L, match(L, '=') ? AUP_TOK_PERCENT_EQUAL : AUP_TOK_PERCENT);
        case '!': return makeToken(L, match(L, '=') ? AUP_TOK_BANG_EQUAL : AUP_TOK_BANG);

        case '-': if (match(L, '>'))    return makeToken(L, AUP_TOK_ARROW);
             else if (match(L, '='))    return makeToken(L, AUP_TOK_MINUS_EQUAL);
                                        return makeToken(L, AUP_TOK_MINUS);

        case '=': if (match(L, '>'))    return makeToken(L, AUP_TOK_ARROW);
             else if (match(L, '='))    return makeToken(L, AUP_TOK_EQUAL_EQUAL);
                                        return makeToken(L, AUP_TOK_EQUAL);

        case '<': if (match(L, '<'))    return makeToken(L, AUP_TOK_LESS_LESS);
             else if (match(L, '='))    return makeToken(L, AUP_TOK_LESS_EQUAL);
                                        return makeToken(L, AUP_TOK_LESS);

        case '>': if (match(L, '>'))    return makeToken(L, AUP_TOK_GREATER_GREATER);
             else if (match(L, '='))    return makeToken(L, AUP_TOK_GREATER_EQUAL);
                                        return makeToken(L, AUP_TOK_GREATER);

        case '\'':
        case '\"': return string(L, c);
    }

    return errorToken(L, "Unexpected character.");
}
