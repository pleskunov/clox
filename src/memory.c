#include "memory.h"
#include "object.h"
#include "vm.h"

/* Dynamic memory management.
oldSize   newSize               Operation

0         Non‑zero              Allocate new block.
Non‑zero  0                     Free allocated block.
Non‑zero  Smaller than oldSize  Shrink existing allocated block.
Non‑zero  Larger than oldSize   Grow existing allocated block. */
void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
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

static void freeObject(Obj* object) {
  // Detect the object type.
  switch (object->type) {
    case OBJ_STRING: {
      // If it is a string, assign a new pointer to that string and use to free up the memory.
      ObjString *string = (ObjString*)object;
      FREE_ARRAY(char, string->chars, string->length + 1); // Type: char, ptr: string->chars, length: length + NULL.
      FREE(ObjString, object); // Free up the memory allocated for "metadata".
      break;
    }
  }
}

void freeObjects() {
  Obj *object = vm.objects;
  // Iterate through each object in the linked list and free the memory.
  while (object != NULL) {
    Obj *next = object->next;
    freeObject(object);
    object = next;
  }
}
