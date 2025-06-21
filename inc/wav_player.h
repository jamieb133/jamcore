#pragma once

#include <stdbool.h>
#include <stdatomic.h>
#include <AudioToolbox/AudioToolbox.h>

#include "core_engine.h"
#include "thread_pool.h"

#define AUDIO_FILE_CHUNK_SIZE 4096 // TODO: make this configurable?
#define WAVPLAYER_LOOPING (1 << 0)
#define WAVPLAYER_FINISHED (1 << 1)
#define WAVPLAYER_SEEK (1 << 2)

typedef struct {
    u64 totalFrames;
    _Atomic(u32) currentFrame;
    _Atomic(u32) seekPosition;
    _Atomic(u8) flags;
    _Atomic(u8) currentBufferIndex;
    _Atomic(u32) currentNumFrames[2];
    f32 buffers[AUDIO_FILE_CHUNK_SIZE][2];
    ExtAudioFileRef audioFile;
    u32 id; // For debug
    ThreadPool* threadPool;

    AudioBufferList* coreAudioBuffers[2];
} WavPlayer;

u16 WavPlayer_Create(WavPlayer* player, CoreEngineContext* ctx, const char* filename, u8 flags);
void WavPlayer_Seek(WavPlayer* player, u32 seekPosition);

