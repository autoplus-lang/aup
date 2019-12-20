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

static aupTk makeToken(int type)
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

static int checkKeyword(int start, int length, const char *rest, int type)
{
	if (lexer.current - lexer.start == start + length &&
		memcmp(lexer.start + start, rest, length) == 0) {
		return type;
	}

	return TOKEN_IDENTIFIER;
}

static int identifierType()
{
	switch (lexer.start[0]) {
		case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
		case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
		case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
		case 'f':
			if (lexer.current - lexer.start > 1) {
				switch (lexer.start[1]) {
					case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
					case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
					case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
				}
			}
			break;
		case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
		case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
		case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
		case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
		case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
		case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
		case 't':
			if (lexer.current - lexer.start > 1) {
				switch (lexer.start[1]) {
					case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
					case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
				}
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

static aupTk number()
{
	while (isDigit(peek())) advance();

	// Look for a fractional part.             
	if (peek() == '.' && isDigit(peekNext())) {
		// Consume the ".".                      
		advance();

		while (isDigit(peek())) advance();
	}

	return makeToken(TOKEN_NUMBER);
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
	if (isDigit(c)) return number();

	switch (c) {
		case '(': return makeToken(TOKEN_LEFT_PAREN);
		case ')': return makeToken(TOKEN_RIGHT_PAREN);
		case '{': return makeToken(TOKEN_LEFT_BRACE);
		case '}': return makeToken(TOKEN_RIGHT_BRACE);
		case ';': return makeToken(TOKEN_SEMICOLON);
		case ',': return makeToken(TOKEN_COMMA);
		case '.': return makeToken(TOKEN_DOT);
		case '-': return makeToken(TOKEN_MINUS);
		case '+': return makeToken(TOKEN_PLUS);
		case '/': return makeToken(TOKEN_SLASH);
		case '*': return makeToken(TOKEN_STAR);
		case '!':
			return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
		case '=':
			return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
		case '<':
			return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
		case '>':
			return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);

		case '\'':
		case '\"': return string(c);
	}

	return errorToken("Unexpected character.");
}