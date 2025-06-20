#pragma once

#include <stdbool.h>
#include <stdatomic.h>
#include <AudioToolbox/AudioToolbox.h>
#include "core_engine.h"

typedef struct {
    _Atomic(u64) currentFrame;
    _Atomic(u64) totalFrames;
    bool looping;
    AudioBufferList* coreAudioBuffers;
} WavPlayer;

u16 WavPlayer_Create(WavPlayer* player, CoreEngineContext* ctx, const char* filename, bool looping);

