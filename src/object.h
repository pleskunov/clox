#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

/* Object type enum. */
typedef enum {
  OBJ_STRING,
} ObjType;

/* Object "header" struct shared by all obejcts types. */
struct Obj {
  ObjType     type;
  struct Obj  *next; // We store objects as a linked list.
};

/* A string type object. */
struct ObjString {
  Obj   obj;
  int   length;
  char  *chars;
};

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

/* Extract the object type tag from a given Value. */
#define OBJ_TYPE(value)   (AS_OBJ(value)->type)

/* Check if a given Value has the string type. */
#define IS_STRING(value)  isObjType(value, OBJ_STRING)

/* Take a Value that is expected to contain a pointer to a valid ObjString on the heap.
The first returns the ObjString* pointer. The second one steps through that to return 
the character array itself. */
#define AS_STRING(value)  ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*)AS_OBJ(value))->chars)

#endif
