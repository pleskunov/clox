#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "object.h"
#include <stdint.h>

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

typedef struct {
  Token name;
  int   depth;
  bool  isCaptured;
} Local;

typedef struct {
  uint8_t   index;
  bool      isLocal; // Controls whether the closure captures a local variable or an upvalue from the surrounding function.
} Upvalue;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT
} FunctionType;

/* Local variables are ordered in the array in the order that their declarations appear in the code. */
typedef struct Compiler {
  struct Compiler *enclosing;
  ObjFunction     *function;
  FunctionType    type;                   // Indicates when we are compiling top-level code vs the body of a function.
  Local           locals[UINT8_COUNT];
  int             localCount;             // How many of locals are in scope.
  Upvalue         upvalues[UINT8_COUNT];  // Closed-over local variable’s slot index array.
  int             scopeDepth;             // Number of blocks surrounding the current bit of code being compiled.
} Compiler;

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
static int resolveLocal(Compiler *compiler, Token *name);
static int resolveUpvalue(Compiler *compiler, Token *name);
static void and_(bool canAssign);
static uint8_t argumentList();

Parser parser;
Compiler *current = NULL;

static Chunk* currentChunk() {
  return &current->function->chunk;
}

/* An intermediary global variable and corresponding function to pass the chunk 
from front end (compile() function) to the compiler's internal sub-routines. */
Chunk *compilingChunk;

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

/* Emit a new loop instruction, which unconditionally jumps backwards by a given offset. */
static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->entries - loopStart + 2;
  if (offset > UINT16_MAX) {
    error("Loop body too large.");
  }

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

/* Emit an instruction, followed by an offest placeholder for backpatching, then update the IP pointer. */
static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->entries - 2;
}

/* To print the result, we temporarily use the OP_RETURN instruction.
So we have the compiler add one of those to the end of the chunk. */
static void emitReturn() {
  emitByte(OP_NIL);
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

/* Backpatching: This goes back into the bytecode and replaces the operand 
at the given location with the calculated jump offset. */
static void patchJump(int offset) {
  // -2 to adjust for the bytecode for the jump offset itself.
  int jump = currentChunk()->entries - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->function = newFunction();
  current = compiler;

  if (type != TYPE_SCRIPT) {
    current->function->name = copyString(parser.previous.start, parser.previous.length);
  }

  /* Claims stack slot zero for the VM’s internal use. No longer accessible to user. */
  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  local->isCaptured = false;
  local->name.start = "";
  local->name.length = 0;
}

static ObjFunction* endCompiler() {
  emitReturn();
  ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
  }
#endif
  current = current->enclosing;
  return function;
}

/* Enter new scope level */
static void beginScope() {
  current->scopeDepth++;
}

static void endScope() {
  current->scopeDepth--;

  while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
    if (current->locals[current->localCount - 1].isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
    current->localCount--;
  }
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

static void call(bool canAssign) {
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
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

static void or_(bool canAssign) {
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);

  parsePrecedence(PREC_OR);
  patchJump(endJump);
}

/* Extract the string’s characters directly from the lexeme, trim the leading and trailing quotation 
marks, then create a string object, wrap it in a Value, and stuffs it into the constant table. */
static void string(bool canAssign) {
  emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  /*
  Before compiling an expression that can also be used as an assignment target, 
  we look for a subsequent = token. If we see one, we compile it as an assignment or 
  setter instead of a variable access or getter.
  */
  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(setOp, (uint8_t)arg);
  } else {
    emitBytes(getOp, (uint8_t)arg);
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
  [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
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
  [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
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

static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length) {
    return false;
  }
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];

    if (identifiersEqual(name, &local->name)) {
      /* If the variable has the sentinel depth, it must be a reference
      to a variable in its own initializer, and we report that as an error. */
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }
      return i;
    }
  }
  // Not found
  return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;
  // Check for multiple variable cross-reference by a closure
  for (int i = 0; i < upvalueCount; i++) {
    Upvalue *upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount == UINT8_COUNT) {
    error("Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;

  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name) {
  // Reached the outermost function without finding a local variable, the variable must be global.
  if (compiler->enclosing == NULL) {
    return -1;
  }
  // Try to resolve the identifier as a local variable in the enclosing compiler
  // by looking for it right outside the current function.
  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t)local, true);
  }

  // Allow a closure to capture either a local variable or an existing upvalue
  // in the immediately enclosing function.
  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t)upvalue, false);
  }

  return -1;
}

/* Initializes the next available Local in the compiler’s array of variables.
It stores the variable’s name and the depth of the scope that owns the variable. */
static void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
  local->isCaptured = false;
}

/* The point where the compiler records the existence of the variable. */
static void declareVariable() {
  if (current->scopeDepth == 0) {
    return;
  }
  Token *name = &parser.previous;

  /* Detect two variables with the same name in the same local scope. 

  Local variables are appended to the array when they’re declared -> they are at the end of the array.
  Start at the end and work backward, looking for an existing variable with the same name.
  */
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }
    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

  addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage) {
  /* It requires the next token to be an identifier, which it consumes and passes over to identfierConstant(). */
  consume(TOKEN_IDENTIFIER, errorMessage);
  
  /* Declare the variable, then exit if we are at the local scope. */
  declareVariable();
  if (current->scopeDepth > 0) {
    return 0;
  }

  return identifierConstant(&parser.previous);
}

/* “Declaring” is when the variable is added to the scope, and “defining” is when it becomes available for use. */
static void markInitialized() {
  if (current->scopeDepth == 0) {
    return;
  }
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
  /* Emit the code to store a local variable if we’re in a local scope. */
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }

  /* We store the string in the constant table and the instruction then refers to the name by its index in the table. 
T his outputs the bytecode instruction that defines the new variable and stores its initial value. */
  emitBytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList() {
  uint8_t argCount = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (argCount == 255) {
        error("Can't have more than 255 arguments.");
      }
      argCount++;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return argCount;
}

/* When this is called, left expression is already evaluated and the result is stored on top of the stack. If the result is 
falsey, we discard the right side operand and left-hand side value as the result of the entire expression. Otherwise, we discard
the left-hand value and evaluate the right operand which becomes the result of the whole AND expression. */
static void and_(bool canAssign) {
  int endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  parsePrecedence(PREC_AND);

  patchJump(endJump);
}

static ParseRule* getRule(TokenType type) {
  // Returns the rule at the given index.
  return &rules[type];
}

static void expression() {
  // Keep compiling until ASSIGNMENT operator precdence level is reached.
  parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
  /* Keep parsing declarations and statements until it hits the closing brace. */
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope(); 

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters.");
      }
      uint8_t constant = parseVariable("Expect parameter name.");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
  block();

  ObjFunction *function = endCompiler();
  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void funDeclaration() {
  uint8_t global = parseVariable("Expect function name.");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
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

static void ifStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition."); 

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();

  /* After executing the then branch, this jumps to the next
  statement after the else branch. This prevents a fallthrough during execution
  of then branch. This jump is always taken. */
  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELSE)) {
    statement();
    patchJump(elseJump);
  }
}

static void forStatement() {
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (match(TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    /* The initializer must have zero effect on the stack state, so we call
    expressionStatement() instead of expression(). It looks for a semicolon,
    which we need here too, and also emits an OP_POP instruction to discard 
    the end value. */
    expressionStatement();
  }

  int loopStart = currentChunk()->entries;
  int exitJump = -1;

  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    // Jump out of the loop if the condition is false.
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Condition.
  }

  /* We can’t compile the increment clause later, since the compiler 
  only makes a single pass over the code. Instead, we’ll jump over the
  increment, run the body, jump back up to the increment, run it, and
  then go to the next iteration. */
  if (!match(TOKEN_RIGHT_PAREN)) {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->entries;
    expression();
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyJump);
  }

  statement();
  emitLoop(loopStart);

  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP); // Condition.
  }

  endScope();
}

/* For the print token, compile the rest of the statement. */
static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}

static void returnStatement() {
  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitByte(OP_RETURN);
  }
}

static void whileStatement() {
  int loopStart = currentChunk()->entries;

  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  /* A jump instruction to skip over the subsequent body statement 
  if the condition is falsey. */
  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);
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
  if (match(TOKEN_FUN)) {
    funDeclaration();
  } else if (match(TOKEN_VAR)) {
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
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else {
    expressionStatement();
  }
}

/* This is an entrypoint for compilation; retruns true if no errors were encountered at compilation time, otherwise - false. */
ObjFunction* compile(const char *source) {
  initScanner(source);                              // A call back to initialize scanner.

  /* Initialize the compiler */
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  parser.hadError = false;
  parser.panicMode = false;

  advance();

  /* Now, a program is a sequence of declarations. */
  while (!match(TOKEN_EOF)) {
    declaration();
  }

  ObjFunction* function = endCompiler();
  return parser.hadError ? NULL : function;
}
