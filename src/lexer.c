#include <stdio.h>
#include <string.h>

#include "code.h"

struct Lexer {
    const char *start;
    const char *current;
    const char *lineStart;
    int line;
    int position;
};

static THREAD_LOCAL struct Lexer L;

void aup_initLexer(const char *source)
{
    L.start = source;
    L.current = source;
    L.lineStart = source;

    L.line = 1;
    L.position = 1;
}

static inline bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c == '_')
        || (c == '$');
}

static inline bool isDigit(char c)
{
    return (c >= '0' && c <= '9');
}

static inline bool isHexDigit(char c)
{
    return isDigit(c)
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F');
}

static bool isAtEnd()
{
    return *L.current == '\0';
}

static char advance()
{
    L.current++;
    L.position++;
    return L.current[-1];
}

static char peek()
{
    return *L.current;
}

static char peekNext()
{
    if (isAtEnd()) return '\0';
    return L.current[1];
}

static void newLine()
{
    L.line++;
    L.position = 0;
    L.lineStart = L.current + 1;
}

static void tabIndent()
{
    L.position += 4 - 1;
}

static bool match(char expected)
{
    if (isAtEnd()) return false;
    if (*L.current != expected) return false;

    L.current++;
    L.position++;
    return true;
}

static aupTok makeToken(aupTTok type)
{
    aupTok token;
    token.type = type;
    token.start = L.start;
    token.length = (int)(L.current - L.start);
    token.line = L.line;
    token.column = L.position - token.length;
    token.lineStart = L.lineStart;

    return token;
}

static aupTok errorToken(const char *message)
{
    aupTok token = makeToken(AUP_TOK_ERROR);
    token.start = message;

    return token;
}

static void skipWhitespace()
{
    for (;;) {
        switch (peek()) {           
            case '\t':
                tabIndent();
                goto _adv;
            case '\n':
                newLine();
                goto _adv;
            case ' ':
            case '\r':
            _adv:
                advance();
                break;

            case ';':
                goto _slc;
            case '/':
                if (peekNext() == '/') {
                    // Single-line comments
                _slc:
                    while (peek() != '\n' && !isAtEnd()) {
                        advance();
                    }
                }
                else if (peekNext() == '*') {
                    // Multi-line comments
                    advance();
                    advance();
                    while (true) {
                        while (peek() != '*' && !isAtEnd()) {
                            if (advance() == '\n') {
                                newLine();
                            }
                        }
                        if (peekNext() == '/') {
                            break;
                        }
                        advance();
                    }
                    advance();
                    advance();
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

static aupTTok checkKeyword(int start, int length, const char *rest, aupTTok type)
{
    if (L.current - L.start == start + length &&
        memcmp(L.start + start, rest, length) == 0) {
        return type;
    }

    return AUP_TOK_IDENTIFIER;
}

static aupTTok identifierType()
{
#define START   (L.start)
#define LENGTH  (L.current - L.start)
#define CHECK(s, l, r, t) \
    checkKeyword(s, l, r, AUP_TOK_KW_##t)

    switch (START[0]) {
        case 'a': return CHECK(1, 2, "nd",   AND);
        case 'b': return CHECK(1, 4, "reak", BREAK);
        case 'c':
            if (LENGTH > 1) switch (START[1]) {
                case 'a': return CHECK(2, 2, "se",  CASE);
                case 'l': return CHECK(2, 3, "ass", CLASS);
            }
            break;
        case 'd': return CHECK(1, 1, "o",    DO);
        case 'e': return CHECK(1, 3, "lse",  ELSE);
        case 'f':
            if (LENGTH > 1) switch (START[1]) {
                case 'a': return CHECK(2, 3, "lse", FALSE);
                case 'o': return CHECK(2, 1, "r",   FOR);
                case 'u': return CHECK(2, 2, "nc",  FUNC);
            }
            break;
        case 'i': return CHECK(1, 1, "f", IF);
        case 'm': return CHECK(1, 4, "atch", MATCH);
        case 'n':
            if (LENGTH > 1) switch (START[1]) {
                case 'o': return CHECK(2, 1, "t",  NOT);
                case 'i': return CHECK(2, 1, "l", NIL);
            }
            break;
        case 'o': return CHECK(1, 1, "r",     OR);
        case 'p': return CHECK(1, 3, "uts",   PUTS);
        case 'r': return CHECK(1, 5, "eturn", RETURN);
        case 's': return CHECK(1, 5, "witch", SWITCH);
        case 't':
            if (LENGTH > 1) switch (START[1]) {
                case 'h': 
                    if (LENGTH > 2) switch (START[2]) {
                        case 'e':return CHECK(3, 1, "n", THEN);
                        case 'i':return CHECK(3, 1, "s", THIS);
                    }
                    break;
                case 'r': return CHECK(2, 2, "ue", TRUE);
            }
            break;
        case 'v': return CHECK(1, 2, "ar",   VAR);
        case 'w': return CHECK(1, 4, "hile", WHILE);
    }
#undef START
#undef LENGTH
#undef CHECK

    return AUP_TOK_IDENTIFIER;
}

static aupTok identifier()
{
    while (isAlpha(peek()) || isDigit(peek())) {
        advance();
    }

    return makeToken(identifierType());
}

static aupTok number()
{
    if (L.start[0] == '0' && isAlpha(peek())) {
        switch (peek()) {
            case 'x':
            case 'X': {
                advance();
                while (isAlpha(peek()) || isDigit(peek())) {
                    if (!isHexDigit(peek())) {
                        return errorToken("Expect hexadecimal digit.");
                    }
                    advance();
                }
                if (L.current - L.start <= 2) {
                    return errorToken("Expect hexadecimal digit.");
                }
                return makeToken(AUP_TOK_HEXADECIMAL);
            }
            default:
                return errorToken("Unknow number format.");
        }
    }

    while (isDigit(peek())) {
        advance();
    }

    // Look for a fractional part.             
    if (peek() == '.' && isDigit(peekNext())) {
        // Consume the point
        advance();

        while (isDigit(peek())) {
            advance();
        }

        return makeToken(AUP_TOK_NUMBER);
    }

    return makeToken(AUP_TOK_INTEGER);
}

static aupTok string()
{
    char open = L.start[0];

    while (peek() != open) {
        if (isAtEnd() || peek() == '\n') {
            return errorToken("Unterminated string.");
        }
        advance();
    }

    advance();
    return makeToken(AUP_TOK_STRING);
}

aupTok aup_scanToken()
{
    skipWhitespace();

    L.start = L.current;

    if (isAtEnd()) return makeToken(AUP_TOK_EOF);

    char c = advance();
    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c) {
        case '(': return makeToken(AUP_TOK_LPAREN);
        case ')': return makeToken(AUP_TOK_RPAREN);
        case '[': return makeToken(AUP_TOK_LBRACKET);
        case ']': return makeToken(AUP_TOK_RBRACKET);
        case '{': return makeToken(AUP_TOK_LBRACE);
        case '}': return makeToken(AUP_TOK_RBRACE);

        case ';': return makeToken(AUP_TOK_SEMICOLON);
        case ',': return makeToken(AUP_TOK_COMMA);
        case '.': return makeToken(AUP_TOK_DOT);
        case ':': return makeToken(AUP_TOK_COLON);
        case '?': return makeToken(match('.') ? AUP_TOK_QMARK_DOT : AUP_TOK_QMARK);

        case '~': return makeToken(AUP_TOK_TILDE);
        case '^': return makeToken(match('=') ? AUP_TOK_CARET_EQUAL : AUP_TOK_CARET);

        case '&': return makeToken(
            match('&') ? AUP_TOK_KW_AND :
            match('=') ? AUP_TOK_AMPERSAND_EQUAL : AUP_TOK_AMPERSAND);
        case '|': return makeToken(
            match('|') ? AUP_TOK_KW_OR :
            match('=') ? AUP_TOK_VBAR_EQUAL : AUP_TOK_VBAR);

        case '+': return makeToken(match('=') ? AUP_TOK_PLUS_EQUAL : AUP_TOK_PLUS);
        case '/': return makeToken(match('=') ? AUP_TOK_SLASH_EQUAL : AUP_TOK_SLASH);
        case '%': return makeToken(match('=') ? AUP_TOK_PERCENT_EQUAL : AUP_TOK_PERCENT);
        case '!': return makeToken(match('=') ? AUP_TOK_BANG_EQUAL : AUP_TOK_BANG);

        case '-':
            if      (match('>')) return makeToken(AUP_TOK_ARROW);
            else if (match('=')) return makeToken(AUP_TOK_MINUS_EQUAL);
                                 return makeToken(AUP_TOK_MINUS);
        case '=':
            if      (match('>')) return makeToken(AUP_TOK_ARROW);
            else if (match('=')) return makeToken(AUP_TOK_EQUAL_EQUAL);
                                 return makeToken(AUP_TOK_EQUAL);
        case '*':
            if      (match('*')) return makeToken(AUP_TOK_STAR_STAR);
            else if (match('=')) return makeToken(AUP_TOK_STAR_EQUAL);
                                 return makeToken(AUP_TOK_STAR);
        case '<':
            if      (match('<')) return makeToken(AUP_TOK_LESS_LESS);
            else if (match('=')) return makeToken(AUP_TOK_LESS_EQUAL);
                                 return makeToken(AUP_TOK_LESS);
        case '>':
            if      (match('>')) return makeToken(AUP_TOK_GREATER_GREATER);
            else if (match('=')) return makeToken(AUP_TOK_GREATER_EQUAL);
                                 return makeToken(AUP_TOK_GREATER);

        case '\'':
        case '\"': return string();
    }

    return errorToken("Unexpected character.");
}

aupTok aup_peekToken(int n)
{
    if (n <= 0) n = 1;

    struct Lexer backup = L;
    aupTok token;
    
    for (int i = 0; i < n; i++)
        token = aup_scanToken();

    L = backup;
    return token;
}
