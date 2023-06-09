/* This module is a stream-line scanner to convert the source code into the tokens.  */

#ifndef clox_scanner_h
#define clox_scanner_h

typedef enum TokenType_ {
  /* Single-character tokens. */
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
  TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
  /* One- or two-character tokens. */
  TOKEN_BANG, TOKEN_BANG_EQUAL,
  TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER, TOKEN_GREATER_EQUAL,
  TOKEN_LESS, TOKEN_LESS_EQUAL,
  /* Literals. */
  TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
  /* Keywords. */
  TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
  TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
  TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
  TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,
  /* A synthetic “error” and EOF tokens. */
  TOKEN_ERROR, TOKEN_EOF
} TokenType;

/* A token produced by the Scanner. */
typedef struct Token_ {
  TokenType   type;
  const char  *start;
  int         length;
  int         line;
} Token;

/* Initialize the Scanner. */
void initScanner(const char *source);

/* Request a new token from the Scanner. Returns one Token at a time. */
Token scanToken();

#endif
