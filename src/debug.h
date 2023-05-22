#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

/* Disassemble the bytecode chunk. */
void disassembleChunk(Chunk *chunk, const char *name);

/* Disassemble an instruction at the given offset.
Returns the offset of the next instruction. */
int disassembleInstruction(Chunk *chunk, int offset);

#endif
