#ifndef LUMEN_DEBUG_H
#define LUMEN_DEBUG_H

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(const Chunk* chunk, int offset);


#endif