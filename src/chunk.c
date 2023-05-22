#include <stdint.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void initChunk(Chunk *chunk) {
  chunk->capacity = 0;
  chunk->entries = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->entries + 1) {
    // Not enough space, increase capacity of the array
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);

    // Reallocate memory, line numbers array must grow in parallel to the bytecode array
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
  }

  // Write bytecode and the corresponding line number, update entries counter
  chunk->code[chunk->entries] = byte;
  chunk->lines[chunk->entries] = line;
  chunk->entries++;
}

void freeChunk(Chunk *chunk) {
  // Free memory for both arrays
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(int, chunk->lines, chunk->capacity);

  // Re-init to deal with dangling pointers
  freeValueArray(&chunk->constants);
  initChunk(chunk);
}

int addConstant(Chunk *chunk, Value value) {
  writeValueArray(&chunk->constants, value);
  return chunk->constants.entries - 1;
}
