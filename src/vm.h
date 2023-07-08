/* This module is a virtual machine that executes the bytecode instructions. */

#ifndef clox_vm_h
#define clox_vm_h

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "object.h"
#include "table.h"

#define STACK_MAX 256

/* A VM registers. */
typedef struct {
  Chunk     *chunk;
  uint8_t   *ip;                // Instruction pointer.
  Value     stack[STACK_MAX];
  Value     *stackTop;          // Just past the element containing the top value.
  Table     globals;
  Table     strings;
  Obj       *objects;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM(void);

void freeVM(void);

InterpretResult interpret(const char *source);

void push(Value value);

Value pop(void);

#endif
