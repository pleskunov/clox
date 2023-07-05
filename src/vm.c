#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "compiler.h"
#include "value.h"
#include "vm.h"

VM vm;

/* Helpers */
static void resetStack(void) {
  // Set a pointer to the beginning of the array, to indicate that the stack is empty
  vm.stackTop = vm.stack;
}

/* Report the runtime error. */
static void runtimeError(const char *format, ...) {

  /* Determine a number of arguments that were passed at the function call. */
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args); // stderr stream is used as buffer to compile the message to print.
  va_end(args);
  fputs("\n", stderr);

  /* Look up the offending line. The interpreter advances past each instruction before executing it,
  so we need to shift it back by 1. */
  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = vm.chunk->lines[instruction];

  fprintf(stderr, "[line %d] in script\n", line);

  resetStack();
}

void initVM(void) {
  resetStack();
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

/* Return a Value from the stack without poping it. */
static Value peek(int distance) {
  // The distance determines how far down from the top of the stack to look: zero is the top, one is one slot down, etc.
  return vm.stackTop[-1 - distance];
}

/* Returns true if a given value is NIL or boolean false; otherwise - false. */
static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void freeVM(void) {
}

static InterpretResult run(void) {

// Macro to read the bytecode pointed by IP
#define READ_BYTE() (*vm.ip++)
// Macro to read a constant from the next byte after bytecode
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
// Marco to handle operations that use binary operators
#define BINARY_OP(valueType, operator) \
  do { \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
      runtimeError("Operands must be numbers."); \
      return INTERPRET_RUNTIME_ERROR; \
    } \
    double rhs_operand = AS_NUMBER(pop()); \
    double lhs_operand = AS_NUMBER(pop()); \
    push(valueType(lhs_operand operator rhs_operand)); \
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

      case OP_NIL:        push(NIL_VAL);            break;
      case OP_TRUE:       push(BOOL_VAL(true));     break;
      case OP_FALSE:      push(BOOL_VAL(false));    break;
      case OP_EQUAL: {
        Value rhs_operand = pop();
        Value lhs_operand = pop();
        push(BOOL_VAL(valuesEqual(lhs_operand, rhs_operand)));
        break;
      }
      case OP_GREATER:    BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS:       BINARY_OP(BOOL_VAL, <); break;
      case OP_ADD:        BINARY_OP(NUMBER_VAL, +); break; // The wrapper to use is passed in as a macro parameter.
      case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY:   BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE:     BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT:
        push(BOOL_VAL(isFalsey(pop())));
        break;
      case OP_NEGATE:
        /* Check if the Value on top of the stack is a number. If it’s not, report the runtime error and terminate. */
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
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
  /* Create a new chunk to store compiled bytecode and initialize it. */
  Chunk chunk;
  initChunk(&chunk);

  /* Attempt to compile the source code. If an error is encountered at
  compilation time (compile() -> false), discard unusable chunk, then return
  the corresponding error. */
  if(!compile(source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  /* Send the completed chunk over to the VM to be executed. */
  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;
  InterpretResult rc = run();

  freeChunk(&chunk);
  return rc;
}
