#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NEGATE,
  OP_RETURN,
} OpCode;

/* Bytecode instructions struct. */
typedef struct {
  int         capacity;
  int         entries;
  uint8_t     *code;        // -> array of opcodes
  int         *lines;       // -> array of line numbers
  ValueArray  constants;    // -> struct to handle constants
} Chunk;

/* Initialize the bytecode chunk. */
void initChunk(Chunk *chunk);

/* Append the BYTE read from the LINE to the CHUNK. */
void writeChunk(Chunk *chunk, uint8_t byte, int line);

/* Destroy the existing chunk. */
void freeChunk(Chunk *chunk);

/* Add the constant to the constant pool. Returns index 
of element to which the constant was appended.*/
int addConstant(Chunk *chunk, Value value);

#endif
