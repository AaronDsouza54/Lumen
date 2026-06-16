#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "memory.h"
#include "object.h"
#include "value.h"

void initValueArray(ValueArray* array) {
    array->values = null;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        const int32_t oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values,
                                   oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void printValue(Value value) {
    switch (value.type) {
    case VAL_BOOL:
          printf(AS_BOOL(value) ? "true" : "false");
          break;
    case VAL_NULL: printf("null"); break;
    case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
    case VAL_OBJ: printObject(value); break;
    case VAL_INTEGER: printf("%" PRId64, AS_INTEGER(value)); break;
    }
}

bool valuesEqual(Value a, Value b) {
    if (a.type == b.type) {
        switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);

        case VAL_NULL:
            return true;

        case VAL_INTEGER:
            return AS_INTEGER(a) == AS_INTEGER(b);

        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);

        case VAL_OBJ:
            return AS_OBJ(a) == AS_OBJ(b);

        default:
            return false; // shoul be unreachable
        }
    }

    // Handling comparison between int and double types
    if ((a.type == VAL_INTEGER && b.type == VAL_NUMBER)) {
        return (double)AS_INTEGER(a) == AS_NUMBER(b);
    } else if ((a.type == VAL_NUMBER && b.type == VAL_INTEGER)) {
        return AS_NUMBER(a) == (double)AS_INTEGER(b);
    }

    return false;
}


