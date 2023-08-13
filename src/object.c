#include "memory.h"
#include "object.h"
#include "vm.h"
#include "table.h"

/* A wrapper for allocateObject() to avoid manual typecatsing and calculate the size. */
#define ALLOCATE_OBJ(type, objectType) (type*)allocateObject(sizeof(type), objectType)

/* Allocate object of a given size and type. */
static Obj* allocateObject(size_t size, ObjType type) {
  /* Create new "header" object and set its type. */
  Obj *object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;

  /* Update pointers in the VM registers: next <= current, current <= new. */
  object->next = vm.objects;
  vm.objects = object;

  return object;
}

ObjClosure* newClosure(ObjFunction *function) {
  // Create a dynamic array to store upvalues and initilize its memory
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);

  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;

  
  return closure;
}

ObjFunction* newFunction() {
  ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjNative* newNative(NativeFn function) {
  ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  return native;
}

/* Create new object of type string by calling macro wrapper, initialize string object fields
(a string length and a pointer to that string) and return the pointer to the string object. */
static ObjString* allocateString(char *chars, int length, uint32_t hash) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  /* Automatically intern every string. */
  tableSet(&vm.strings, string, NIL_VAL);
  return string;
}

/* String hashing function, the algorithm is FNV-1a. */
static uint32_t hashString(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjString* takeString(char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  /* Look up the string in the string table first. If it is found, before returning it, 
  we must free the memory for the string that was passed in. */
  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(chars, length, hash);
}

/* Allocate new memory block on heap, copy given array of characters from lexeme to
that memory block, append the terminating chararcter to the end and pass to the
string object constructor function. */
ObjString* copyString(const char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  /* Look up a new string being created in the string table first. If it is found, 
  instead of “copying”, just return a reference to that string. */
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    return interned;
  }
  /* Otherwise, allocate a new string, and store it in the string table. */
  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';

  return allocateString(heapChars, length, hash);
}

static void printFunction(ObjFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

ObjUpvalue* newUpvalue(Value* slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->closed = NIL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;

  return upvalue;
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE(value)->function);
      break;
    case OBJ_FUNCTION:
      printFunction(AS_FUNCTION(value));
      break;
    case OBJ_NATIVE:
      printf("<native fn>");
      break;
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
    case OBJ_UPVALUE:
      printf("upvalue");
      break;
  }
}
