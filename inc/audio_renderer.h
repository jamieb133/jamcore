#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <types.h>
#include "core_engine.h"

#define AUDIO_RENDERER_RECORDING (1 << 0)
#define AUDIO_RENDERER_MUTE (1 << 1)

typedef struct {
    AudioStreamBasicDescription streamFormat;
    ExtAudioFileRef outputFile;
    AudioBufferList* coreAudioBuffers[2];
    atomic_u8 currentBufferIndex;
    atomic_u8 flags;
    atomic_u16 seekPosition;
    atomic_u16 numFramesToWrite;
    u16 framesThisCycle;
    ThreadPool* threadPool;
} AudioRenderer;

u16 AudioRenderer_Create(AudioRenderer* renderer, CoreEngineContext* ctx, const char* filename);
void AudioRenderer_StartRecord(AudioRenderer* renderer);
void AudioRenderer_StopRecord(AudioRenderer* renderer);

