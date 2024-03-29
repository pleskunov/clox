#include <stdint.h>
#include <stdio.h>

#include "object.h"
#include "chunk.h"
#include "value.h"
#include "debug.h"

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1; // simple, update offset by 1 byte
}

/* The local variable’s name never leaves the compiler to make it into the chunk at all.
When disassembling these instructions, we can’t show the variable’s name, just the slot number. */
static int byteInstruction(const char* name, Chunk* chunk, int offset) {
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2; 
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
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
    case OP_NIL:
      return simpleInstruction("OP_NIL", offset);
    case OP_TRUE:
      return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
      return simpleInstruction("OP_FALSE", offset);
    case OP_POP:
      return simpleInstruction("OP_POP", offset);
    case OP_GET_LOCAL:
      return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
      return byteInstruction("OP_SET_LOCAL", chunk, offset);
    case OP_GET_GLOBAL:
      return constantInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL:
      return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
      return constantInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_GET_UPVALUE:
      return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
      return byteInstruction("OP_SET_UPVALUE", chunk, offset);
    case OP_EQUAL:
      return simpleInstruction("OP_EQUAL", offset);
    case OP_GREATER:
      return simpleInstruction("OP_GREATER", offset);
    case OP_LESS:
      return simpleInstruction("OP_LESS", offset);
    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
      return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
      return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
      return simpleInstruction("OP_DIVIDE", offset);
    case OP_NOT:
      return simpleInstruction("OP_NOT", offset);
    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);
    case OP_PRINT:
      return simpleInstruction("OP_PRINT", offset);
    case OP_JUMP:
      return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
      return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    case OP_LOOP:
      return jumpInstruction("OP_LOOP", -1, chunk, offset);
    case OP_CALL:
      return byteInstruction("OP_CALL", chunk, offset);
    case OP_CLOSURE: {
      offset++;
      uint8_t constant = chunk->code[offset++];
      printf("%-16s %4d ", "OP_CLOSURE", constant);
      printValue(chunk->constants.values[constant]);
      printf("\n");
      ObjFunction *function = AS_FUNCTION(chunk->constants.values[constant]);
      for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d      |                     %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
      }
      return offset;
    }
    case OP_CLOSE_UPVALUE:
      return simpleInstruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    default:
      // Handler if a given byte is not an instruction
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}
