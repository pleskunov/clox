#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"

/* Macro to build up a dynamic array capacity. */
#define GROW_CAPACITY(capacity) \
  ((capacity) < 8 ? 8 : (capacity) * 2)

/* Macro wrapper for reallocate() to expand the array to the new capacity. */
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
  (type *)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

/* Macro wrapper for reallocate() to free the array. */
#define FREE_ARRAY(type, pointer, oldCount) \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

/* Dynamic memory management.
oldSize   newSize               Operation

0         Non‑zero              Allocate new block.
Non‑zero  0                     Free allocated block.
Non‑zero  Smaller than oldSize  Shrink existing allocated block.
Non‑zero  Larger than oldSize   Grow existing allocated block.
*/
void *reallocate(void *pointer, size_t oldSize, size_t newSize);

#endif
