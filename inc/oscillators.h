#pragma once

#include <stdatomic.h>
#include "core_engine.h"

typedef enum {
    WAVEFORM_SIN,
    WAVEFORM_SQUARE,
    WAVEFORM_SAW,

    WAVEFORM_COUNT,
} WaveformId;

typedef struct {
    _Atomic(WaveformId) type;
    _Atomic(f64) frequency;
    _Atomic(f64) phase;
    _Atomic(f64) amplitude;
} Oscillator;

u16 Oscillator_Create(Oscillator* osc, 
                      CoreEngineContext* ctx,
                      WaveformId type,
                      f64 frequency,
                      f64 phase,
                      f64 amplitude);

