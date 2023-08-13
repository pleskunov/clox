/* This module is a virtual machine that executes the bytecode instructions. */

#ifndef clox_vm_h
#define clox_vm_h

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "object.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
  ObjClosure  *closure;
  uint8_t     *ip;
  Value       *slots;           // The first slot that a function can use.
} CallFrame;

/* A VM registers. */
typedef struct {
  CallFrame   frames[FRAMES_MAX];
  int         frameCount;
  Value       stack[STACK_MAX];
  Value       *stackTop;          // Just past the element containing the top value.
  Table       globals;
  Table       strings;
  ObjUpvalue  *openUpvalues;
  Obj         *objects;
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
