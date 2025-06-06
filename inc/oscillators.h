#pragma once

#include "core_engine.h"

typedef enum {
    WAVEFORM_SIN,
    WAVEFORM_SQUARE,
    WAVEFORM_SAW,

    WAVEFORM_COUNT,
} WaveformId;

typedef struct {
    WaveformId type;
    double frequency;
    double phase;
} Oscillator;

void Oscillator_Create(Oscillator* osc, 
                        WaveformId type,
                        double frequency,
                        double phase,
                        CoreEngineContext* engine, 
                        JamAudioChannelId channelId);

