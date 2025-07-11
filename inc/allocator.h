#pragma once

#include <stdlib.h>
#include <stdbool.h>

#include <logger.h>
#include <types.h>

// TODO: allocate from global arena or bump allocator

#define AllocRange(type, size) (type*)calloc(size, sizeof(type))
#define AllocOne(type) (type*)calloc(1, sizeof(type))
#define Dealloc(data) Assert(data, "Tried to deallocate null data"); free(data); data = NULL

// Utils
#define ZeroMem(type, ptr, size) memset(ptr, 0, sizeof(type) * size)
#define ZeroObj(type, ptr) memset(ptr, 0, sizeof(type))

#define AssertMemZeroed(type, data, size, format, ...) {\
    unsigned char __zeros[sizeof(type) * size] = { 0, };\
    Assert(memcmp((const void*)__zeros, (const void*)data, sizeof(type) * size) == 0, format, ##__VA_ARGS__);\
}

typedef struct {
    u8* base;
    u32 offset, size;
} ScratchAllocator;

void ScratchAllocator_Init(ScratchAllocator* alloc, u8* base, u32 size);
void* ScratchAllocator_Alloc(ScratchAllocator* alloc, u32 size);
void* ScratchAllocator_Calloc(ScratchAllocator* alloc, u32 size);
void ScratchAllocator_Release(ScratchAllocator* alloc);

