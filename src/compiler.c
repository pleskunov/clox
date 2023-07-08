#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"

#ifdef DEBUG_PRINT_CODE
  #include "debug.h"
#endif

/* A compiler registers. */
typedef struct Parser_ {
  Token   current;
  Token   previous;

  bool    hadError;   // Indicates whether any errors occurred during compilation.
  bool    panicMode;  // Tracks whether compiler is currently in panic mode.
} Parser;

/* A oprator precedence numerical definition: C implicitly gives successively 
larger numbers for enums, this means that PREC_CALL is numerically > than PREC_UNARY. */
typedef enum Precedence_ {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;

// A placeholder function for ParserRule table.
typedef void (*ParseFn)(bool canAssign);

/* A ParseRule table structure. */
typedef struct ParseRule_ {
  ParseFn       prefix;       // A slot for prefix operator compiling functions.
  ParseFn       infix;        // A slor for infix operator compiling functions.
  Precedence    precedence;   // Operator precedence level identifier.
} ParseRule;

/* Forward declarations to handle the fact that the grammar is recursive. */
static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static uint8_t identifierConstant(Token* name);

Parser parser;

/* An intermediary global variable and corresponding function to pass the chunk 
from front end (compile() function) to the compiler's internal sub-routines. */
Chunk *compilingChunk;

static Chunk* currentChunk() {
  return compilingChunk;
}

/* ERROR HANDLING */
static void errorAt(Token *token, const char *message) {
  /* Error cascade prevention: suppress any error that get detected. */
  if (parser.panicMode) {
    return;
  }

  parser.panicMode = true; // Enable panic mode when an error has occured.

  /* Display a location in the source code where encountered error is found. */
  fprintf(stderr, "[Line %d] Error", token->line);
  if (token->line == TOKEN_EOF) {
    fprintf(stderr, " at end");
  }
  else if (token->line == TOKEN_ERROR) {
    // Nothing to display.
  }
  else {
    // Try to display the lexeme if it’s human-readable.
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }
  /* Print the error message and set hadError flag. */
  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

/* Report an error at the location of the token just consumed. */
static void error(const char* message) {
  errorAt(&parser.previous, message);
}

/* Pull the location out of the current token in order to tell the 
user where the error occurred and pass it to errorAt(). */
static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

/* COMPILER FRONT END */
/* Ask the scanner for the next token and store it for later use. */
static void advance() {
  /* Stash the current token into previous field of parser state struct, before reading new one. */
  parser.previous = parser.current;

  /* Keep reading tokens and reporting the errors, until a non-error token is found.
  This ensures that the backned of the parser sees only valid tokens. */
  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR) {
      break;
    }
    // Pass control to handle errors during the compilation.
    errorAtCurrent(parser.current.start);
  }
}

/* Similar to advance(). Read the next token, validate that the token has an expected type.
If not, report an error. */
static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }
  errorAtCurrent(message);
}

/* Return true if the token has a given type. */
static bool check(TokenType type) {
  return parser.current.type == type;
}

static bool match(TokenType type) {
  if (!check(type)) {
    return false;
  }
  /* If the current token has the given type, we consume the token and return true. */
  advance();
  return true;
}

/* BYTECODE GENERATION */
/* Append a single byte (opcode or operand to an instruction) to the chunk.
Send the previous token’s line info so that runtime errors are associated with that line. */
static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

/* Write an opcode followed by a one-byte operand. */
static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

/* To print the result, we temporarily use the OP_RETURN instruction.
So we have the compiler add one of those to the end of the chunk. */
static void emitReturn() {
  emitByte(OP_RETURN);
}

/* Insert an entry to the constant table. */
static uint8_t makeConstant(Value value) {
  /* Add the given value to the end of the chunk’s constant table and return its index. */
  int constant = addConstant(currentChunk(), value);

  /* Make sure the number of constants does not exceed the limit of the stack (256). */
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }
  /* Return constant index. */
  return (uint8_t)constant;
}

/* Generate the code to load converted the number literal value to the VM stack. */
static void emitConstant(Value value) {
  // Write opcode, followed by the index where the constant value is stored.
  emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler() {

#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "code");
  }
#endif

  emitReturn();
}

/* PARSERS FOR TOKENS */
static void binary(bool canAssign) {
  /* At the moment of call entire left-hand operand expression has already 
  been compiled and the subsequent infix operator consumed. */
  // Grab the operator token type.
  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));
  // Emit the corresponding instruction.
  switch (operatorType) {
    case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
    case TOKEN_GREATER:       emitByte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS:          emitByte(OP_LESS); break;
    case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS:          emitByte(OP_ADD); break;
    case TOKEN_MINUS:         emitByte(OP_SUBTRACT); break;
    case TOKEN_STAR:          emitByte(OP_MULTIPLY); break;
    case TOKEN_SLASH:         emitByte(OP_DIVIDE); break;
    default: 
      return; // Unreachable.
  }
}
static void literal(bool canAssign) {
  switch (parser.previous.type) {
    case TOKEN_FALSE: emitByte(OP_FALSE); break;
    case TOKEN_NIL:   emitByte(OP_NIL);   break;
    case TOKEN_TRUE:  emitByte(OP_TRUE);  break;
    default:
      return; // Unreachable.
  }
}

/* Recursively call back into expression() to compile the expression between the
parentheses, then parse the closing ) at the end. 

This function has no runtime semantics on its own and therefore doesn’t emit any bytecode.
The inner call to expression() takes care of generating bytecode for the expression inside the parentheses. */
static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

/* A function to compile number literals. */
static void number(bool canAssign) {
  // Convert a lexeme to a double value using C std. library, then pass it to the code gen function.
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value)); // Convert to Lox internal representation and set the tag.
}

/* Extract the string’s characters directly from the lexeme, trim the leading and trailing quotation 
marks, then create a string object, wrap it in a Value, and stuffs it into the constant table. */
static void string(bool canAssign) {
  emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
  uint8_t arg = identifierConstant(&name);

  /*
  Before compiling an expression that can also be used as an assignment target, 
  we look for a subsequent = token. If we see one, we compile it as an assignment or 
  setter instead of a variable access or getter.
  */
  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(OP_SET_GLOBAL, arg);
  } else {
    emitBytes(OP_GET_GLOBAL, arg);
  }
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

/* A function to compile unary operators. */
static void unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;

  // Compile the operand (grab the token type to detect which unary operator it is).
  parsePrecedence(PREC_UNARY);

  // Emit the operator instruction.
  switch (operatorType) {
    case TOKEN_BANG:  emitByte(OP_NOT); break;
    case TOKEN_MINUS: emitByte(OP_NEGATE); break;
    // Here, will be other unary operator cases...
    default: return; // Unreachable.
  }
}

/* The parser rule table for dynamic lookup of precedence level and expressions-specific functions. */
ParseRule rules[] = {
  // [TOKEN TYPE]       = {PREFIX,   INFIX,  PRECEDENCE}
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

/* Start at the current token and parse any expression at the given precedence level or higher. */
static void parsePrecedence(Precedence precedence) {

  // Request for next token.
  advance();

  // Look up the corresponding struct by token type, resolve prefix handling function.
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;

  // If it is null, we have an error. 
  if (prefixRule == NULL) {
    error("Expect expression.");
    return;
  }

  // Pass the control to prefix parsing function, to emit the bytecode instructions.
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  /* Keep looping, we look for an infix parser for the next token while the precdence
  is greater than the treshold one.

  1) If we find one, it means the prefix expression compiled might be an operand for it.
  But only if the call to parsePrecedence() has a precedence that is low enough to permit that infix operator.

  2) If the next token is too low precedence, or isn’t an infix operator at all, we’re done.
  */


  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    // Consume the operator and hand off control to the infix parser we found.
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

/* Take the given token and add its lexeme to the chunk’s constant table as a string. 
It then returns the index of that constant in the constant table. */
static uint8_t identifierConstant(Token* name) {
  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static uint8_t parseVariable(const char* errorMessage) {
  /* It requires the next token to be an identifier, which it consumes and passes over to identfierConstant(). */
  consume(TOKEN_IDENTIFIER, errorMessage);
  return identifierConstant(&parser.previous);
}
/* We store the string in the constant table and the instruction then refers to the name by its index in the table. 
This outputs the bytecode instruction that defines the new variable and stores its initial value. */
static void defineVariable(uint8_t global) {
  emitBytes(OP_DEFINE_GLOBAL, global);
}

static ParseRule* getRule(TokenType type) {
  // Returns the rule at the given index.
  return &rules[type];
}

static void expression() {
  // Keep compiling until ASSIGNMENT operator precdence level is reached.
  parsePrecedence(PREC_ASSIGNMENT);
}

static void varDeclaration() {
  uint8_t global = parseVariable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    /* If the user doesn’t initialize the variable, we implicitly initialize it to nil. */
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(global);
}

/* An “expression statement” is simply an expression followed by a semicolon.
Semantically, an expression statement evaluates the expression and discards the result. */
static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
}

/* For the print token, compile the rest of the statement. */
static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}

/* Error synchronization. */
static void synchronize() {
  parser.panicMode = false;

  /* Skip tokens indiscriminately until a statement boundary is reached. */
  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) {
      return;
    }
    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default:
        ; // Do nothing.
    }
    advance();
  }
}

static void declaration() {
  if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statement();
  }

  /* If we hit a compile error while parsing the previous statement, we are now in panic mode.
  When that happens, after the statement we start synchronizing.*/
  if (parser.panicMode) {
    synchronize();
  }
}

static void statement() {
  /* Match the print statemnt. */
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else {
    expressionStatement();
  }
}

/* This is an entrypoint for compilation; retruns true if no errors were encountered at compilation time, otherwise - false. */
bool compile(const char *source, Chunk *chunk) {
  initScanner(source);                              // A call back to initialize scanner.

  /* Pass the adress of a given chunk to compiler internal sub-routines. */
  compilingChunk = chunk;

  /* Initialize the sentinel flags before attemting to parse and compile the tokens. */
  parser.hadError = false;
  parser.panicMode = false;

  advance();

  /* Now, a program is a sequence of declarations. */
  while (!match(TOKEN_EOF)) {
    declaration();
  }

  endCompiler();

  return !parser.hadError;                          // Return false if hadError flag is set at compilation time.
}
