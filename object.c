#include <stdio.h>
#include <string.h>

#include "common.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

static void printFunction(const ObjFunction* function);

#define ALLOCATE_OBJ(type, objectType) \
    (type*) allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;

  object->next = vm.objects;
  vm.objects = object;
  return object;
}

static ObjString* allocateString(char* chars, int32_t length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tableSet(&vm.strings, string, NULL_VAL);
    return string;
}

static uint32_t hashString(const char* key, int32_t length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjString* copyString(const char* chars, const int32_t length) {
  const uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  
  if (interned != NULL) return interned;

  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

void printObject(const Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
          printf("%s", AS_CSTRING(value));
          break;
        case OBJ_TABLE:
          ObjTable* table = (ObjTable*)AS_OBJ(value);
          printf("{");
          bool first = true;


          for (int i = 0; i < table->table->capacity; i++) {
            Entry* entry = &table->table->entries[i];
            if (entry == NULL) continue;

            if (!first) printf(",");

            first = false;

            printValue(OBJ_VAL(entry->key));
            printf(": ");
            printValue(entry->value);
          }

          printf("}");
          break;
        case OBJ_FUNCTION:
          printFunction(AS_FUNCTION(value));
          break;
        case OBJ_NATIVE:
          printf("<native function>");
          break;
        case OBJ_CLOSURE:
          printFunction(AS_CLOSURE(value)->function);
          break;
    }
}

ObjString* takeString(char* chars, const int32_t length) {
  const uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(chars, length, hash);
}

static void printFunction(const ObjFunction* function) {
  if (function->name == null) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

ObjTable* newTableObject() {
    ObjTable* table = ALLOCATE_OBJ(ObjTable, OBJ_TABLE);
    initTable(table->table);
    return table;
}

ObjFunction* newFunction() {
  ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjNative* newNative(NativeFn function) {
  ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  return native;
}

ObjClosure* newClosure(ObjFunction* function) {
  ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  return closure;
}