#include "memory.h"
#include "object.h"
#include "table.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void freeTable(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
  /* Calculate the bucket index by maping the key’s hash code to this index within the array’s bounds. */
  uint32_t index = key->hash % capacity;
  Entry *tombstone = NULL;

  for (;;) {
    Entry *entry = &entries[index];
    /*
    If the key for the Entry at that array index is NULL -> the bucket is empty.
      LOOK UP -> miss, there is no element
      INSERT -> hit, we found a free bucket to add new Entry

    If the key in the bucket = the key we’re looking for, then that key is already present in the table.
      LOOK UP -> hit, e’ve found the key we seek.
      INSERT -> we’ll be replacing the value for that key instead of adding a new entry.

    Otherwise, the bucket has an entry in it, but with a different key -> Collision. Start probing.

    Loop is used for linear displacement.
    */
    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) {
        // Empty entry.
        return tombstone != NULL ? tombstone : entry;
      } else {
        // We found a tombstone.
        if (tombstone == NULL) {
          tombstone = entry;
        }
      }
    } else if (entry->key == key) {
      // We found the key.
      return entry;
    }
    /* Second modulo operator wraps us back around to the beginning. */
    index = (index + 1) % capacity;
  }
}

bool tableGet(Table* table, ObjString* key, Value* value) {
  if (table->count == 0) { 
    return false;
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) {
    return false;
  }

  *value = entry->value;
  return true;
}

static void adjustCapacity(Table* table, int capacity) {
  /* Allocate the bucket array, initialize every element to be an empty bucket 
  and then store that array (and its capacity) in the hash table’s main struct. */
  Entry *entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  /* During the resizing of the array, we need to recalculate its size, so clear it out. */
  table->count = 0;

  /* Walk through the old array front to back.
  Any time we find a non-empty bucket, we insert that entry into the new array */
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    if (entry->key == NULL) { 
      continue; 
    } // Restart the loop.

    Entry *dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    /* Count only for entries, not the toombstones. */
    table->count++; 
  }
  /* Free the memory allocated for the old array. */
  FREE_ARRAY(Entry, table->entries, table->capacity);
  /* Update the table pointers. */
  table->entries = entries;
  table->capacity = capacity;
}

bool tableSet(Table* table, ObjString* key, Value value) {
  /* Allocate enough memory to store the new entry. */
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }
  /* Look up a bucket for the given entry. */
  Entry *entry = findEntry(table->entries, table->capacity, key);
  /* Check if bucket is empty and only update table's size if it is. 
  Do not increase the count if we overwrote the value for an already-present key. */
  bool isNewKey = entry->key == NULL;
  if (isNewKey && IS_NIL(entry->value)) {
    /* Toombstones are treated as full entries. */
    table->count++;
  }
  /* Copy the key and value into the corresponding fields in the Entry. */
  entry->key = key;
  entry->value = value;
  return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
  if (table->count == 0) {
    return false;
  }
  // Find the entry.
  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) {
    return false;
  }
  // Place a tombstone in the entry.
  entry->key = NULL;
  entry->value = BOOL_VAL(true);
  return true;
}

/* Walk the bucket array of the source hash table. Whenever we find a non-empty bucket,
we add the entry to the destination hash table. */
void tableAddAll(Table* from, Table* to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry *entry = &from->entries[i];
    if (entry->key != NULL) {
      tableSet(to, entry->key, entry->value);
    }
  }
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
  if (table->count == 0) {
    return NULL;
  }
  uint32_t index = hash % table->capacity;

  for (;;) {
    Entry *entry = &table->entries[index];
    if (entry->key == NULL) {
      // Stop if we find an empty non-tombstone entry.
      if (IS_NIL(entry->value)) {
        return NULL;
      }
      /* Look at the actual strings. First, check if they have matching lengths and hashes.
         If there is a hash collision, we do an actual character-by-character string comparison.
      */
    } else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0) {
      // We found it.
      return entry->key;
    }
    index = (index + 1) % table->capacity;
  }
}
