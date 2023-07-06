/* This module handles low-level operations with memory at runtime. */

#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"

/* Allocate a memory block of given size and type. */
#define ALLOCATE(type, count) \
  (type*)reallocate(NULL, 0, sizeof(type) * (count))

/* Free the memory block pointed by a given pointer. */
#define FREE(type, pointer) \
  reallocate(pointer, sizeof(type), 0)

/* Expand the capacity of a dynamic array. */
#define GROW_CAPACITY(capacity) \
  ((capacity) < 8 ? 8 : (capacity) * 2)

/* Resize the dynamic array. */
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
  (type *)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

/* Free the memory allocated for the dynamic array. */
#define FREE_ARRAY(type, pointer, oldCount) \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

/* Dynamic memory management. See memory.c for more details. */
void *reallocate(void *pointer, size_t oldSize, size_t newSize);

/* Free the memory allocated for objects in a heap at the runtime. */
void freeObjects();

#endif
