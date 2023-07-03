#include "common.h"
#include "scanner.h"
#include <string.h>

typedef struct Scanner_ {
  const char *start;      // a lexeme being scanned
  const char *current;    // a character being scanned
  int line;
} Scanner;

Scanner scanner;

void initScanner(const char *source) {
  scanner.start = source;     // <- 1st lexeme
  scanner.current = source;   // <- 1st char of 1st lexeme
  scanner.line = 1;           // <- 1st line in source code
}

static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c) {
  return c >= '0' && c <= '9';
}

static bool isAtEnd() {
  // If the current character is the null byte, then we’ve reached the end.
  return *scanner.current == '\0';
}

static char advance() {
  // Update a pointer position to the next character in the source, then return "consumed" character.
  scanner.current++;
  return scanner.current[-1];
}

static char peek() {
  // Simply return the character, not consuming it.
  return *scanner.current;
}

static char peekNext() {
  if (isAtEnd()) {
    return '\0';
  }
  // Return one character past the current one.
  return scanner.current[1];
}

static bool match(char expected) {
  // If EOF is reached or there is no character match, return false.
  if (isAtEnd()) {
    return false;
  }
  if (*scanner.current != expected) {
    return false;
  }
  // If the character matches what we expect, consume it and return true.
  scanner.current++;
  return true;
}

/* A constructor function to assemble a new token. */
static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  return token;
}

/* A constructor function to assemble an error token. */
static Token errorToken(const char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  return token;
}

static void skipWhitespace() {
  // Get rid of any spaces, tabs and newlines - they are not part of lexeme.
  // Look for any matches, advance scanner past any leading whitespace.
  for (;;) {
    char c = peek();

    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
      case '\n':
        // Consume new lines and update the line counter.
        scanner.line++; 
        advance();
        break;
      /* Comments are not, technically, whitespaces, but for the compiler they are,
      so check for any forward slashes to detect commented lines and consume them. */
      case '/':
        if (peekNext() == '/') {
          // Continue untill EOL is reached.
          while (peek() != '\n' && !isAtEnd()) {
            advance();
          }
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

static TokenType checkKeyword(int start, int length, const char *rest, TokenType type) {
  if (scanner.current - scanner.start == start + length && \
    memcmp(scanner.start + start, rest, length) == 0) 
  {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {

  switch (scanner.start[0]) {
    case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
    case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
    case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
    case 'f':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'a': 
            return checkKeyword(2, 3, "lse", TOKEN_FALSE);
          case 'o':
            return checkKeyword(2, 1, "r", TOKEN_FOR);
          case 'u':
            return checkKeyword(2, 1, "n", TOKEN_FUN);
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
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'h':
            return checkKeyword(2, 2, "is", TOKEN_THIS);
          case 'r':
            return checkKeyword(2, 2, "ue", TOKEN_TRUE);
        }
      }
      break;
    case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
    case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
  }

  return TOKEN_IDENTIFIER;
}

static Token identifier() {
  while (isAlpha(peek())|| isDigit(peek())) {
    advance();
  }
  return makeToken(identifierType());
}

static Token number() {
  while (isDigit(peek())) {
    advance();
  }

  // Look for a fractional part of the number being scanned.
  if (peek() == '.' && isDigit(peekNext())) {
    // Consume ".", then keep scanning.
    advance();

    while (isDigit(peek())) {
      advance();
    }
  }

  return makeToken(TOKEN_NUMBER);
}

static Token string() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() != '\n') {
      scanner.line++;
    }
    advance();
  }

  if (isAtEnd()) {
    return errorToken("Unterminated string");
  }

  // Consume characters until the closing quote is reached.
  advance();
  return makeToken(TOKEN_STRING);
}

Token scanToken() {
  skipWhitespace();
  /* Because scanToken() is always called to scan a complete token,
  we set scanner.start to point to a current character, so we "remember" 
  where the lexeme we’re about to scan begins. */
  scanner.start = scanner.current; 

  if (isAtEnd()) {
    return makeToken(TOKEN_EOF);
  }

  char c = advance();

  if (isAlpha(c)) {
    return identifier();
  }
  if (isDigit(c)) {
    return number();
  }

  switch(c) {
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
    case '!': return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=': return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"': return string();
  }

  return errorToken("Unexpected character.");
}
