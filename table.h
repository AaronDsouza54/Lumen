#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

typedef struct {
  ObjString* key;
  Value value;
} Entry;


typedef struct {
    int32_t count;
    int32_t capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(const Table* table, ObjString* key, Value* value);
bool tableSet(Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int32_t length, uint32_t hash);

#endif