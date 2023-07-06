#include "memory.h"
#include "object.h"
#include "vm.h"

/* A wrapper for allocateObject() to avoid manual typecatsing and calculate the size. */
#define ALLOCATE_OBJ(type, objectType) \
  (type*)allocateObject(sizeof(type), objectType)

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

/* Create new object of type string by calling macro wrapper, initialize string object fields
(a string length and a pointer to that string) and return the pointer to the string object. */
static ObjString* allocateString(char *chars, int length) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;

  return string;
}

ObjString* takeString(char* chars, int length) {
  return allocateString(chars, length);
}

/* Allocate new memory block on heap, copy given array of characters from lexeme to
that memory block, append the terminating chararcter to the end and pass to the
string object constructor function. */
ObjString* copyString(const char* chars, int length) {
  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';

  return allocateString(heapChars, length);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
  }
}
