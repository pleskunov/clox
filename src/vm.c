#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "compiler.h"
#include "value.h"
#include "vm.h"

#include <stdio.h>

VM vm;

/* Helpers */
static void resetStack(void) {
  // Set a pointer to the beginning of the array, to indicate that the stack is empty
  vm.stackTop = vm.stack;
}


void initVM(void) {
  resetStack();
}

void freeVM(void) {
}

static InterpretResult run(void) {

// Macro to read the bytecode pointed by IP
#define READ_BYTE() (*vm.ip++)
// Macro to read a constant from the next byte after bytecode
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
// Marco to handle operations that use binary operators
#define BINARY_OP(operator) \
  do { \
    double rhs_operand = pop(); \
    double lhs_operand = pop(); \
    push(lhs_operand operator rhs_operand); \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    // Tracing stack content subroutine
    printf("          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");

    /* OFFSET = (INT) CURRENT INSTR ADDR - FIRST INSTR IN CHUNK */
    disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      /* decoding the instruction: opcode -> implementation */
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_ADD: 
        BINARY_OP(+);
        break;
      case OP_SUBTRACT:
        BINARY_OP(-);
        break;
      case OP_MULTIPLY:
        BINARY_OP(*);
        break;
      case OP_DIVIDE:
        BINARY_OP(/);
        break;
      case OP_NEGATE:
        push(-pop()); // this increments/decrements the stack pointer unnecessary
        break;
      case OP_RETURN: {
        printValue(pop());
        printf("\n");
        return INTERPRET_OK;
      }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
  compile(source);
  return INTERPRET_OK;
}

void push(Value value) {
  // Push the value to the top of the stuck, move top stack pointer up
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop(void) {
  // Move stack pointer down, deference and return the value
  vm.stackTop--;
  return *vm.stackTop;
}
