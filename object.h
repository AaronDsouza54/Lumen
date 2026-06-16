#ifndef LUMEN_OBJECT_H
#define LUMEN_OBJECT_H

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

typedef enum {
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_STRING,
    OBJ_TABLE,
} ObjType;

struct Obj {
    ObjType type;
    Obj* next;
};

typedef struct {
    Obj obj;
    int32_t arity;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

struct ObjString {
    Obj obj;
    int32_t length;
    char* chars;
    uint32_t hash;
};

struct ObjTable {
    Obj obj;
    Table* table;
};

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

typedef struct {
    Obj obj;
    ObjFunction* function;
} ObjClosure;

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)

#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
#define AS_CLOSURE(value)     ((ObjClosure*)AS_OBJ(value))

ObjString* copyString(const char* chars, int length);
ObjString* takeString(char* chars, int length);
void printObject(Value value);
ObjFunction* newFunction();
ObjNative* newNative(NativeFn function);
ObjClosure* newClosure(ObjFunction* function);


static inline bool isObjType(const Value value, const ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif