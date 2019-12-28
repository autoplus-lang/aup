#include <stdio.h>
#include <string.h>

#include "compiler.h"

typedef struct {
	const char *start;
	const char *current;
	int line;
	int position;
} aupLx;

static aupLx lexer;

void aupLx_init(const char *source)
{
	lexer.start = source;
	lexer.current = source;
	lexer.line = 1;
	lexer.position = 1;
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

static bool isBinaryDigit(char c)
{
	return (c == '0')
		|| (c == '1');
}

static bool isOctalDigit(char c)
{
	return (c >= '0')
		&& (c <= '7');
}

static bool isHexadecimalDigit(char c)
{
	return (c >= '0' && c <= '9')
		|| (c >= 'a' && c <= 'f')
		|| (c >= 'A' && c <= 'F');
}

static bool isAtEnd()
{
	return *lexer.current == '\0';
}

static void newLine()
{
	lexer.line++;
	lexer.position = 0;
}

static void tabIndent()
{
	lexer.position += (4 - 1);
}

static char advance()
{
	lexer.current++;
	lexer.position++;
	return lexer.current[-1];
}

static char peek()
{
	return *lexer.current;
}

static char peekNext()
{
	if (isAtEnd()) return '\0';
	return lexer.current[1];
}

static bool match(char expected)
{
	if (isAtEnd()) return false;
	if (*lexer.current != expected) return false;

	lexer.current++;
	return true;
}

static aupTk makeToken(aupTkt type)
{
	aupTk token;
	token.type = type;
	token.start = lexer.start;
	token.length = (int)(lexer.current - lexer.start);
	token.line = lexer.line;
	token.column = lexer.position - token.length;

	return token;
}

static aupTk errorToken(const char *message)
{
	aupTk token;
	token.type = TOKEN_ERROR;
	token.start = message;
	token.length = (int)strlen(message);
	token.line = lexer.line;
	token.column = lexer.position - 1;

	return token;
}

static void skipWhitespace()
{
	for (;;) {
		char c = peek();
		switch (c) {
			case '\t':
				tabIndent();
			case ' ':
			case '\r':
				advance();
				break;

			case '\n':
				newLine();
				advance();
				break;

			case '/':
				if (peekNext() == '/') {
					// A comment goes until the end of the line.   
					while (peek() != '\n' && !isAtEnd()) advance();
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

static aupTkt checkKeyword(int start, int length, const char *rest, aupTkt type)
{
	if (lexer.current - lexer.start == start + length &&
		memcmp(lexer.start + start, rest, length) == 0) {
		return type;
	}

	return TOKEN_IDENTIFIER;
}

static int identifierType()
{
#define INDEX   lexer.start
#define LENGTH  (lexer.current - lexer.start)

	switch (INDEX[0]) {
		case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
		case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
		case 'e':
            if (LENGTH > 1) switch (INDEX[1]) {				
		        case 'l': return checkKeyword(2, 2, "se", TOKEN_ELSE);
                case 'n': return checkKeyword(2, 1, "d", TOKEN_END);
			}
			break;
		case 'f':
			if (LENGTH > 1) switch (INDEX[1]) {
                case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                case 'u': return checkKeyword(2, 2, "nc", TOKEN_FUNC);
            }
			break;
		case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
		case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
		case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
		case 'p': return checkKeyword(1, 3, "uts", TOKEN_PUTS);
		case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
		case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
		case 't':
            if (LENGTH > 1) switch (INDEX[1]) {
                case 'h':
                    if (LENGTH > 2) switch (INDEX[2]) {
                        case 'e': return checkKeyword(3, 1, "n", TOKEN_THEN);
                        case 'i': return checkKeyword(3, 1, "s", TOKEN_THIS);
                    }
                    break;
                case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
            }
			break;
		case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
		case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
	}

	return TOKEN_IDENTIFIER;
}

static aupTk identifier()
{
	while (isAlpha(peek()) || isDigit(peek())) advance();

	return makeToken(identifierType());
}

static aupTk number(char c)
{
	if (c == '0' && isAlpha(peek()) && !isDigit(peek())) {
		switch (advance()) {
			case 'b': case 'B':
				while (isDigit(peek()) || isAlpha(peek()))
					if (!isBinaryDigit(advance())) return errorToken("Expect binary digit.");
				return makeToken(TOKEN_BINARY);

			case 'o': case 'O': case 'q': case 'Q':
				while (isDigit(peek()) || isAlpha(peek()))
					if (!isOctalDigit(advance())) return errorToken("Expect octal digit.");
				return makeToken(TOKEN_OCTAL);

			case 'x': case 'X': case 'h': case 'H':
				while (isDigit(peek()) || isAlpha(peek()))
					if (!isHexadecimalDigit(advance())) return errorToken("Expect hexadecimal digit.");
				return makeToken(TOKEN_HEXADECIMAL);

			default:
				return errorToken("Unexpected number format.");
		}
	}

	while (isDigit(peek())) advance();

	// Look for a fractional part.             
	if (peek() == '.' && isDigit(peekNext())) {
		// Consume the ".".                      
		advance();

		while (isDigit(peek())) advance();

		return makeToken(TOKEN_NUMBER);
	}

	return makeToken(TOKEN_INTEGER);
}

static aupTk string(char c)
{
	while (peek() != c && !isAtEnd()) {
		if (peek() == '\n') newLine();
		advance();
	}

	if (isAtEnd()) return errorToken("Unterminated string.");

	// The closing quote.                                    
	advance();
	return makeToken(TOKEN_STRING);
}

aupTk aupLx_scan()
{
	skipWhitespace();

	lexer.start = lexer.current;

	if (isAtEnd()) return makeToken(TOKEN_EOF);

	char c = advance();
	if (isAlpha(c)) return identifier();
	if (isDigit(c)) return number(c);

	switch (c) {
		case '(': return makeToken(TOKEN_LPAREN);
		case ')': return makeToken(TOKEN_RPAREN);
		case '[': return makeToken(TOKEN_LBRACKET);
		case ']': return makeToken(TOKEN_RBRACKET);
		case '{': return makeToken(TOKEN_LBRACE);
		case '}': return makeToken(TOKEN_RBRACE);

		case ';': return makeToken(TOKEN_SEMICOLON);
		case ',': return makeToken(TOKEN_COMMA);
		case '.': return makeToken(TOKEN_DOT);

		case '-': return makeToken(TOKEN_MINUS);
		case '+': return makeToken(TOKEN_PLUS);
		case '/': return makeToken(TOKEN_SLASH);
		case '*': return makeToken(TOKEN_STAR);
		case '%': return makeToken(TOKEN_PERCENT);

		case '!':
			return makeToken(match('=') ? TOKEN_BANG_EQ : TOKEN_BANG);
		case '=':
			return makeToken(match('=') ? TOKEN_EQUAL_EQ : TOKEN_EQUAL);
		case '<':
			return makeToken(match('=') ? TOKEN_LESS_EQ : TOKEN_LESS);
		case '>':
			return makeToken(match('=') ? TOKEN_GREATER_EQ : TOKEN_GREATER);

		case '\'':
		case '\"': return string(c);
	}

	return errorToken("Unexpected character.");
}
