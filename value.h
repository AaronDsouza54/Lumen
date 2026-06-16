#ifndef LUMEN_VALUE_H
#define LUMEN_VALUE_H

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjTable ObjTable;

typedef enum {
    VAL_BOOL,
    VAL_NULL,
    VAL_NUMBER,
    VAL_INTEGER,
    VAL_OBJ,
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        int64_t integer;
        double number;
        Obj* obj;
    } as;
} Value;

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NULL(value)    ((value).type == VAL_NULL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)
#define IS_INTEGER(value) ((value).type == VAL_INTEGER)

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
#define AS_INTEGER(value) ((value).as.integer)
#define AS_OBJ(value)     ((value).as.obj)

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NULL_VAL           ((Value){VAL_NULL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define INTEGER_VAL(value) ((Value){VAL_INTEGER, {.integer = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

typedef struct {
    int32_t capacity;
    int32_t count;
    Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);


#endif