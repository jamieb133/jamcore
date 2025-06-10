#include <utils.h>
#include <logger.h>

u16 Bitcount(u16 mask) 
{
    return (u16)__builtin_popcount((i32)mask);
}

void BufferSum(f32* buffer, f32 value, u16 size)
{
    // TODO: SIMD optimisation?
    for (u16 i = 0; i < size; i++) {
        buffer[i] += value;
    }
}

void BufferProduct(f32* buffer, f32 value, u16 size)
{
    // TODO: SIMD optimisation?
    for (u16 i = 0; i < size; i++) {
        buffer[i] *= value;
    }
}

void BufferParallelSum(f32* bufferOut, f32* bufferIn, u16 size)
{
    // TODO: SIMD optimisation?
    for (u16 i = 0; i < size; i++) {
        bufferOut[i] += bufferIn[i];
    }
}

f32 ClampHigh(f32 value, f32 max)
{
    return (value > max) ? max : value;
}

f32 ClampLow(f32 value, f32 min)
{
    return (value < min) ? min : value;
}

f32 Clamp(f32 value, f32 min, f32 max)
{
    f32 result = value;
    result = ClampHigh(result, max);
    result = ClampLow(result, min);
    return value;
}

