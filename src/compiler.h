/* This is a compiler module that converts the tokens into the bytecode instructions. */

#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"

/* Parse a source code and output a compiled bytecode instructions to the chunk. */
bool compile(const char *source, Chunk *chunk);

#endif
