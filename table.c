#include <string.h>

#include "common.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = null;
}

void freeTable(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

static Entry* findEntry(Entry* entries, int32_t capacity, ObjString* key) {
  uint32_t index = key->hash % capacity;
  Entry* tombstone = null;
  for (;;) {
    Entry* entry = &entries[index];

    if (entry->key == null) {
        if (IS_NULL(entry->value)) {
            // Empty entry
            return tombstone != null ? tombstone : entry;
        } else {
            if (tombstone == null) tombstone = entry;
        }
    } else if (entry->key == key) {
        // Found the key
        return entry;
    }

    index = (index + 1) % capacity;
  }
}

bool tableGet(const Table* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;

    const Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == null) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(Table* table, const int32_t capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int32_t i = 0; i < capacity; i++) {
        entries[i].key = null;
        entries[i].value = NULL_VAL;
    }


    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == null) continue;

        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);

    table->entries = entries;
    table->capacity = capacity;
}


bool tableSet(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }
    
    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == null;
    if (isNewKey && IS_NULL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != null) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(Table* table, const char* chars, int32_t length, uint32_t hash) {
    if (table->count == 0) return null;

    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == null) {
            // We need to stop if there is a non-empty tombstone
            if (IS_NULL(entry->value)) return null;
        } else if (entry->key->length == length && 
                    entry->key->hash == hash &&
                    memcmp(entry->key->chars, chars, length) == 0) {
            //Implies that the value exists
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}

bool tableDelete(Table* table, ObjString* key) {
  if (table->count == 0) return false;

  // Find the entry.
  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == null) return false;

  // Placing a tombstone in the entry.
  entry->key = null;
  entry->value = BOOL_VAL(true);
  return true;
}
