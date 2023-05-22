#include <stdlib.h>

#include "memory.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    // Extra logic to destroy previously allocated memory block
    free(pointer);
    return NULL;
  }

  // If pointer parameter == NULL -> same as malloc(newSize)
  void *new_array = realloc(pointer, newSize);
  if (new_array == NULL) {
    exit(1);
  }

  return new_array;
}
