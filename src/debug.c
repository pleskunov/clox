#include <stdint.h>
#include <stdio.h>

#include "chunk.h"
#include "debug.h"
#include "value.h"

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1; // simple, update offset by 1 byte
}

void disassembleChunk(Chunk *chunk, const char *name) {
  // A header to identify the chunk being disassembled
  printf("== %s ==\n", name);

  /* Crank through the bytecode array, disassembling each instruction.
  Internal dispatch logic controls the returned offest (points to the next
  instruction), which allows to handle variable size instructions. */
  for (int offset = 0; offset < chunk->entries;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
  // Read the address of the constant from the next byte after the bytecode instruction
  uint8_t constant = chunk->code[offset + 1];

  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");

  return offset + 2; // 2 byte long instruction, update offset accordingly
}

int disassembleInstruction(Chunk *chunk, int offset) {
  // Offset of given instruction
  printf("%04d ", offset);

  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    // Instruction originates from the same line of source code as previous one
    printf("   | ");
  }
  else {
    // Instruction belongs to different source code line
    printf("%4d ", chunk->lines[offset]);
  }

  // Read single byte from the bytecode at the given offset
  uint8_t instruction = chunk->code[offset];

  /* Dispatch logic: for each kind of instruction,
  we call a little utility function to display it */
  switch (instruction) {
    case OP_CONSTANT:
      return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
      return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
      return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
      return simpleInstruction("OP_DIVIDE", offset);
    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    default:
      // Handler if a given byte is not an instruction
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}
