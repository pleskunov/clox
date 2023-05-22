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
  // 6 digit precision, strip trailing zeroes from the fractional part
  printf("%g", value);
}
