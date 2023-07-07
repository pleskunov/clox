#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
  ObjString *key;
  Value     value;
} Entry;

typedef struct {
  int   count;
  int   capacity;
  Entry *entries;
} Table;

void initTable(Table* table);

void freeTable(Table* table);

/* Given a key, look up the corresponding value. If it finds an entry with that key, 
it returns true, otherwise it returns false. If the entry exists, the value output 
parameter points to the resulting value.*/
bool tableGet(Table* table, ObjString* key, Value* value);

/* Add the given key/value pair to the given hash table. If an entry for that key is
already present, the new value overwrites the old value. The function returns true if
a new entry was added. */
bool tableSet(Table* table, ObjString* key, Value value);

bool tableDelete(Table* table, ObjString* key);

/* Copy all of the entries of one hash table into another. */
void tableAddAll(Table* from, Table* to);

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

#endif
