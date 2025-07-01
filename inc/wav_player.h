#pragma once

#include <stdbool.h>
#include <stdatomic.h>
#include <AudioToolbox/AudioToolbox.h>

#include "core_engine.h"
#include "thread_pool.h"

#define WAVPLAYER_LOOPING (1 << 0)
#define WAVPLAYER_FINISHED (1 << 1)
#define WAVPLAYER_SEEK (1 << 2)

typedef struct {
    u64 totalFrames;
    atomic_u32 currentFrame;
    atomic_u32 seekPosition;
    atomic_u8 flags;
    atomic_u8 currentBufferIndex;
    atomic_u32 currentNumFrames[2];
    f32 buffers[AUDIO_FILE_CHUNK_SIZE][2];
    ExtAudioFileRef audioFile;
    u32 id; // For debug
    ThreadPool* threadPool;

    AudioBufferList* coreAudioBuffers[2];
} WavPlayer;

u16 WavPlayer_Create(WavPlayer* player, CoreEngineContext* ctx, const char* filename, u8 flags);
void WavPlayer_Seek(WavPlayer* player, u32 seekPosition);

