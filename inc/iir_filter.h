#pragma once

#include <stdatomic.h>
#include <types.h>
#include <core_engine.h>
#include <thread_pool.h>

#define IIR_RECALCULATE (1 << 0)

typedef enum {
    IIR_LOWPASS,
    IIR_HIGHPASS,
    IIR_BANDPASS,
    IIR_BANDSTOP,
    IIR_HIGH_SHELVE,
    IIR_LOW_SHELVE,

    IIR_FILTER_TYPE_COUNT,
} FilterType;

typedef struct {
    f32 sampleRate;
    _Atomic(FilterType) type;
    atomic_f32 freq;
    atomic_f32 dbGain;
    atomic_f32 qFactor;
    atomic_f32 atten;
    struct {
        f32 a0;
        f32 a1;
        f32 a2;
        f32 b0;
        f32 b1;
        f32 b2;
    } coeffs;
    struct {
        f32 inputs[2];
        f32 outputs[2];
    } buffers[2]; // Two sets for left and right
    atomic_u8 flags;
    ThreadPool* threadPool;
} IirFilter;

u16 IirFilter_Create(IirFilter* filter, 
                     CoreEngineContext* ctx, 
                     f64 sampleRate,
                     FilterType type, 
                     f32 freq,
                     f32 qFactor,
                     f32 atten);

void IirFilter_Recalculate(IirFilter* filter);

