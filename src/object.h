#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"
#include "chunk.h"

/* Object type enum. */
typedef enum {
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_STRING,
} ObjType;

/* Object "header" struct shared by all obejcts types. */
struct Obj {
  ObjType     type;
  struct Obj  *next; // We store objects as a linked list.
};

/* A function class object. */
typedef struct {
  Obj       obj;
  int       arity; // Number of parameters the function expects.
  Chunk     chunk;
  ObjString *name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value* args);

/* A native function class object. */
typedef struct {
  Obj       obj;
  NativeFn  function;
} ObjNative;

/* A string class object. */
struct ObjString {
  Obj     obj;
  int     length;
  char    *chars;
  uint32_t hash;
};

ObjFunction* newFunction();
ObjNative* newNative(NativeFn function);

/* Create a new string object and copy a given string into that object. */
ObjString* copyString(const char *chars, int length);

/* Wrap a given string into new string object. */
ObjString* takeString(char* chars, int length);

/* Print out the content of a given object. */
void printObject(Value value);

/* Check if a given Value tag and nested Obj struct type field are both set to a given type. */
static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#define OBJ_TYPE(value)   (AS_OBJ(value)->type)

#define IS_FUNCTION(value)  isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)    isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)    isObjType(value, OBJ_STRING)

#define AS_STRING(value)    ((ObjString*)AS_OBJ(value))
#define AS_FUNCTION(value)  ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)    (((ObjNative*)AS_OBJ(value))->function)
#define AS_CSTRING(value)   (((ObjString*)AS_OBJ(value))->chars)

#endif
