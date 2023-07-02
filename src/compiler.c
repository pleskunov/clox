#include "common.h"
#include "compiler.h"
#include "scanner.h"

void compile(const char *source) {
  initScanner(source);

  /* A token at a time, thanks to a single token of lookahead by clox grammar. */

  // temporary tests
  int line = 1;
  for (;;) {
    Token token = scanToken();
    if (token.line != line) {
      printf("%d ", token.line);
      line = token.line;
    }
    else {
      printf("   | ");
    }
    /* we need to limit the length of printed string as the lexeme points into
    original source string doesn't have NULL terminator, so we pass the precision as
    an argument.
    */
    printf("%2d '%.*s'\n", token.type, token.length, token.start);

    if (token.type == TOKEN_EOF) {
      break;
    }
  }
}
