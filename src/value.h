#ifndef clox_value_h
#define clox_value_h

#include "common.h"

/* An abstraction layer how constants are represented. */
typedef double Value;

typedef struct {
  int     capacity;
  int     entries;
  Value   *values;
} ValueArray;

/* Initialize the new array of constants. */
void initValueArray(ValueArray *array);

/* Append the constant to the array. */
void writeValueArray(ValueArray *array, Value value);

/* Destroy the array of constants. */
void freeValueArray(ValueArray *array);

/* Print the value stored. */
void printValue(Value value);

#endif
