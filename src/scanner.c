#include "common.h"
#include "scanner.h"

/* A scanner instance structure. The start pointer marks the beginning of the
current lexeme being scanned, and current points to the current character being
looked at. A line field is used to track what line the current lexeme is on. */
typedef struct Scanner_ {
  const char *start;
  const char *current;
  int line;
} Scanner;

Scanner scanner;

void initScanner(const char *source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

/* Returns true if a given character belongs to alhabetical range; otherwise false. */
static bool isAlpha(char c) {
  return  (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* Returns true if a given character belongs to numerical rangle; otherwise false. */
static bool isDigit(char c) {
  return c >= '0' && c <= '9';
}

/* Returns true if the current character is a null terminator; otherwise - false. */
static bool isAtEnd() {
  return *scanner.current == '\0';
}

/* Updates scanner register to the next character in a source stream, 
then return the character which is "consumed". */
static char advance() {
  scanner.current++;
  return scanner.current[-1];
}

/* Return the currently scanned character, do not update the scanner register. */
static char peek() {
  return *scanner.current;
}
/* Return a character past the current one, do not update the scanner register. */
static char peekNext() {
  if (isAtEnd()) { return '\0'; }
  return scanner.current[1];
}

/* Check if a given character matches the currently scanned character.
This helper also updates the Scanner register to the next character. */
static bool match(char expected) {
  if (isAtEnd()) { return false; }
  if (*scanner.current != expected) { return false; }
  scanner.current++;
  return true;
}

/* A constructor functions to assemble normal and error tokens. */
static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  return token;
}

static Token errorToken(const char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  return token;
}

/* Get rid of any spaces, tabs and newlines in the source code stream. */
static void skipWhitespace() {
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
        // Consume new line characters, update the scanner register.
        scanner.line++;
        advance();
        break;
      case '/':
        if (peekNext() == '/') {
          // Continue untill EOL is reached.
          while (peek() != '\n' && !isAtEnd()) { advance(); }
        } else {
          return;
        }
        break;
      default:
        return;
    }
  }
}

/* Check for potential token match with keywords pool. Returns a given keyword token type upon match;
otherwise returns a type for variable. */
static TokenType checkKeyword(int start, int length, const char *rest, TokenType type) {
  // The lexeme must be exactly as long as the keyword AND the n of the following characters must match exactly.
  if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }
  return TOKEN_IDENTIFIER;
}

/* Check for matches between identifiers and reserved keywords. */
static TokenType identifierType() {
  switch (scanner.start[0]) {
    case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
    case 'c': return checkKeyword(1, 4, "lass", TOKEN_CLASS);
    case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
    case 'f':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
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
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
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

/* A constructor function wrapper to handle keywords. */
static Token identifier() {
  while (isAlpha(peek()) || isDigit(peek())) { advance(); }
  return makeToken(identifierType());
}

/* A constructor function wrapper to handle numerical tokens. */
static Token number() {
  while (isDigit(peek())) { advance(); }
  // Look for a fractional part.
  if (peek() == '.' && isDigit(peekNext())) {
    // Consume ".".
    advance();

    while (isDigit(peek())) { advance(); }
  }
  return makeToken(TOKEN_NUMBER);
}

/* A constructor function wrapper to handle strings. */
static Token string() {
  // Consume characters until we reach the closing quote.
  while (peek() != '"' && !isAtEnd()) {
    if (peek() != '\n') { scanner.line++; }
    advance();
  }

  if (isAtEnd()) { return errorToken("Unterminated string"); }

  advance();
  return makeToken(TOKEN_STRING);
}

Token scanToken() {
  skipWhitespace();
  scanner.start = scanner.current;

  if (isAtEnd()) { return makeToken(TOKEN_EOF); }
  char c = advance();

  if (isAlpha(c)) { return identifier(); }
  if (isDigit(c)) { return number(); }

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
