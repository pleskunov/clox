#include "vm.h"
#include "chunk.h"
#include "memory.h"
#include "debug.h"
#include "compiler.h"
#include "object.h"
#include "value.h"

#include <time.h>

VM vm;

static Value clockNative(int argCount, Value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

/* Set a pointer to the beginning of the array, to indicate that the stack is empty. */
static void resetStack(void) {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
}

/* Report the runtime error. */
static void runtimeError(const char *format, ...) {
  /* Determine a number of arguments that were passed at the function call. */
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args); // stderr stream is used as buffer to compile the message to print.
  va_end(args);
  fputs("\n", stderr);

  /* Stack tracing. */
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm.frames[i];
    ObjFunction *function = frame->closure->function;

    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }
  resetStack();
}

static void defineNative(const char* name, NativeFn function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

/* VM boot subroutine. */
void initVM(void) {
  resetStack();
  vm.objects = NULL;

  initTable(&vm.globals);
  initTable(&vm.strings);

  defineNative("clock", clockNative);
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

static bool call(ObjClosure* closure, int argCount) {
  /* Runtime error checking. */
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
    return false;
  }

  /* CallFrame overflow mitigation during a deep call. */
  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }
      default:
        break; // Non-callable object type.
    }
  }
  runtimeError("Can only call functions and classes.");
  return false;
}

static ObjUpvalue* captureUpvalue(Value* local) {
  ObjUpvalue *createdUpvalue = newUpvalue(local);
  return createdUpvalue;
}

/* Returns true if a given value is NIL or boolean false; otherwise - false. */
static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  /* Take both strinbg from the VM stack. */
  ObjString *b = AS_STRING(pop());
  ObjString *a = AS_STRING(pop());
  /* Calculate the length of new string and allocate the memory block for it. */
  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  /* Copy over first string, then the second one right after the first and append a 
  NULL terminator char to the end.*/
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';
  /* Produce new object to contain concatenated string. */
  ObjString *result = takeString(chars, length);
  push(OBJ_VAL(result));
}

/* VM terminating subroutine. */
void freeVM(void) {
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  freeObjects();
}

static InterpretResult run(void) {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
/* Macro to read the bytecode pointed by IP */
#define READ_BYTE() (*frame->ip++)
/* Yank the next two bytes from the chunk and build a 16-bit unsigned integer out of them. */
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
/* Macro to read a constant from the next byte after bytecode. */
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])

/* Compiler never emits instructions to non-string const, so we can read a 
one-byte operand from the bytecode chunk. We treat that as an index into the 
chunk’s constant table and return the string at that index. */
#define READ_STRING() AS_STRING(READ_CONSTANT())

/* Marco to handle operations that use binary operators */
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
    disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
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
      case OP_POP:        pop();                    break; // Pop off the stack and discard.
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);     // Access to a given numbered slot relative to the beginning of that frame.
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        /* Pull the constant table index from the instruction’s operand and get the variable name.
        Then, use that index as a key to look up the variable’s value in the globals hash table. */
        ObjString *name = READ_STRING();
        Value value;
        if (!tableGet(&vm.globals, name, &value)) {
          /* If the key isn’t present in the hash table, it means that global variable has never been defined. */
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        /* Otherwise, we take the value and push it onto the stack. */
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        /* Get the name of the variable from the constant table. Then take the value from the
        top of the stack and store it in a hash table with that name as the key. */
        ObjString *name = READ_STRING();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString *name = READ_STRING();
        if (tableSet(&vm.globals, name, peek(0))) {
          /* 
          The call to tableSet() stores the value in the global variable table even if the variable
          wasn’t previously defined. That fact is visible in a REPL session, since it keeps running
          even after the runtime error is reported. So we take care to delete that zombie value from the table.
          */
          tableDelete(&vm.globals, name); 
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_EQUAL: {
        Value rhs_operand = pop();
        Value lhs_operand = pop();
        push(BOOL_VAL(valuesEqual(lhs_operand, rhs_operand)));
        break;
      }
      case OP_GREATER:    BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS:       BINARY_OP(BOOL_VAL, <); break;
      case OP_ADD: {
        /* To support string concatentaion, ADD instruction dynamically 
        inspects the operands and chooses the right operation. */
        if (IS_STRING(peek(0)) && (IS_STRING(peek(1)))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && (IS_NUMBER(peek(1)))) {
          double rhs_operand = AS_NUMBER(pop());
          double lhs_operand = AS_NUMBER(pop());
          push(NUMBER_VAL(lhs_operand + rhs_operand));
        } else {
          runtimeError("Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
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
      case OP_PRINT: {
        printValue(pop());
        printf("\n");
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) {
          frame->ip += offset;
        }
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
        ObjClosure *closure = newClosure(function);
        push(OBJ_VAL(closure));
        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] = captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_RETURN: {
        Value result = pop();
        vm.frameCount--;
        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char *source) {
  ObjFunction *function = compile(source);
  if (function == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }
  push(OBJ_VAL(function));
  ObjClosure *closure = newClosure(function);
  pop();
  push(OBJ_VAL(closure));
  call(closure, 0);

  return run();
}
