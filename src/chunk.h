#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_LOOP,
  OP_CALL,
  OP_RETURN,
} OpCode;

/* A bytecode instruction chunk. */
typedef struct {
  int         capacity;     // dynamic array capacity
  int         entries;      // number of values actually stored in dynamic array
  uint8_t     *code;        // -> array of opcodes
  int         *lines;       // -> array of line numbers
  ValueArray  constants;    // -> struct to handle constants
} Chunk;

/* Initialize a bytecode chunk. */
void initChunk(Chunk *chunk);

/* Append a given byte and its line information to the chunk. */
void writeChunk(Chunk *chunk, uint8_t byte, int line);

/* Destroy the existing chunk. */
void freeChunk(Chunk *chunk);

/* Add the constant to the constant pool. Returns index of element to which 
the constant was appended.*/
int addConstant(Chunk *chunk, Value value);

#endif
