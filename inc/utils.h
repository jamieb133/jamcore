#pragma once

#include "types.h"

u16 Bitcount(u16 mask) ;
void BufferProduct(f32* buffer, f32 value, u16 size);
void BufferParallelSum(f32* bufferOut, f32* bufferIn, u16 size);
f32 ClampHigh(f32 value, f32 max);
f32 ClampLow(f32 value, f32 min);
f32 Clamp(f32 value, f32 min, f32 max);

