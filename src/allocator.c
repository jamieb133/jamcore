#include "logger.h"
#include <string.h>
#include <allocator.h>

void ScratchAllocator_Init(ScratchAllocator* alloc, u8* base, u32 size)
{
    alloc->base = base;
    alloc->offset = 0;
    alloc->size = size;
}

void* ScratchAllocator_Alloc(ScratchAllocator* alloc, u32 size)
{
    u32 remaining = alloc->size - alloc->offset;
    Assert(remaining >= size, "Not enough scratch space for allocation (requested %d, only had %d", size, remaining);
    u8* ptr = alloc->base + alloc->size - alloc->offset - size;
    alloc->offset += size;
    return (void*)ptr;
}

void* ScratchAllocator_Calloc(ScratchAllocator* alloc, u32 size)
{
    void* ptr = ScratchAllocator_Alloc(alloc, size);
    memset(ptr, 0, size);
    return ptr;
}

void ScratchAllocator_Release(ScratchAllocator* alloc)
{
    alloc->offset = 0;
}
