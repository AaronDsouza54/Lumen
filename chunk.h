#ifndef LUMEN_CHUNK_H
#define LUMEN_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NULL,
    OP_TRUE,
    OP_FALSE,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_INT_DIVIDE,
    OP_PRINT,
    OP_NOT,
    OP_RETURN,
    OP_EQUAL,
    OP_POP,
    OP_JUMP,
    OP_LOOP,
    OP_CALL,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_JUMP_IF_FALSE,
    OP_GREATER,
    OP_LESS,
    OP_BANG_EQUAL,
    OP_GREATER_EQUAL,
    OP_LESS_EQUAL,
    OP_CLOSURE,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE
} OpCode;

typedef struct {
    int32_t count;
    int32_t capacity;
    uint8_t* code;
    int* lines;
    ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);


#endif