#ifndef clox_value_h
#define clox_value_h

#include "common.h"

/* A structure to represent the Value tag. */
typedef enum ValueType_ {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER
} ValueType;

/* An abstraction layer over how values are represented in clox. */
typedef struct Value_ {
  ValueType type;         // Value tag for automatic type detection
  union {                 // Value payload
    bool    boolean;
    double  number;
  } as;
} Value;

/* Return true if the Value has the expected type. */
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)

/* Given a Value of the right type, unwrap it and return the corresponding raw C value. */
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

/* Take a C value of the appropriate type and produce a Value that has the correct type tag and contains the underlying value. */
#define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = value}})

typedef struct {
  int     capacity;
  int     entries;
  Value   *values;
} ValueArray;

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
