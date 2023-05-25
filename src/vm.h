#ifndef clox_vm_h
#define clox_vm_h

#include <stdint.h>

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
  Chunk *chunk;
  uint8_t *ip;            // instruction pointer
  Value stack[STACK_MAX];
  Value *stackTop;        // -> just past the element containing the top value
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

/* Create a new VM instance. */
void initVM(void);

/* Destroy the VM instance. */
void freeVM(void);

/* Interpret the bytecode. */
InterpretResult interpret(const char *source);


void push(Value value);

Value pop(void);

#endif
