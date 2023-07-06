/* This module provides a layer of abstraction layer on how different values are represented in clox. */
#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

/* The value tag. */
typedef enum ValueType_ {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ,
} ValueType;

/* The values. */
typedef struct Value_ {
  ValueType type;
  union {
    bool    boolean;
    double  number;
    Obj     *obj;
  } as;
} Value;

/* Return true if the Value has the expected type. */
#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)

/* Given a Value of the right type, unwrap it and return the corresponding raw C value. */
#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)
#define AS_OBJ(value)       ((value).as.obj)

/* Take a C value of the appropriate type and produce a Value that has the correct type tag and contains the underlying value. */
#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)     ((Value){VAL_OBJ, {.obj = (Obj*)object}})

typedef struct {
  int     capacity;
  int     entries;
  Value   *values;
} ValueArray;

/* Check if the values are equal. */
bool valuesEqual(Value a, Value b);

/* Initialize the new array of constants. */
void initValueArray(ValueArray *array);

/* Append the constant to the array. */
void writeValueArray(ValueArray *array, Value value);

/* Destroy the array of constants. */
void freeValueArray(ValueArray *array);

/* Print the value stored. */
void printValue(Value value);

#endif
