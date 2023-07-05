#include <stdio.h>

#include "memory.h"
#include "value.h"

void initValueArray(ValueArray *array) {
  array->capacity = 0;
  array->entries = 0;
  array->values = NULL;
}

void writeValueArray(ValueArray *array, Value value) {
  if (array->capacity < array->entries + 1) {
    // Not enough space, increase capacity of the array
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);

    // Reallocate memory
    array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
  }

  // Write constant, update entries counter
  array->values[array->entries] = value;
  array->entries++;
}

void freeValueArray(ValueArray *array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  // Re-init to make sure no dangling pointers had left
  initValueArray(array);
}

void printValue(Value value) {
  switch (value.type) {
    case VAL_BOOL:
      printf(AS_BOOL(value) ? "true" : "false");
      break;
    case VAL_NIL: 
      printf("nil"); 
      break;
    case VAL_NUMBER: 
      // 6 digit precision, strip trailing zeroes from the fractional part
      printf("%g", AS_NUMBER(value)); 
      break;
  }
}

/* Check if two operands are equal. */
bool valuesEqual(Value a, Value b) {
  if (a.type != b.type) { return false; }
  switch (a.type) {
    case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:    return true;
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    default:         return false; // Unreachable.
  }
}
