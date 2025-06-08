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
    _Atomic(double) frequency;
    _Atomic(double) phase;
} Oscillator;

JamAudioProcessor* Oscillator_Create(Oscillator* osc, 
                                        WaveformId type,
                                        double frequency,
                                        double phase,
                                        CoreEngineContext* engine);

