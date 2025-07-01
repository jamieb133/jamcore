#pragma once

#include "allocator.h"
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <MacTypes.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudioTypes/CoreAudioBaseTypes.h>
#include <stdint.h>

#include <types.h>
#include <thread_pool.h>

#define MAX_PROCESSORS 4096
#define MAX_TASKS 256

#define STACK_ARENA_SIZE_KB 512
#define DEFAULT_HEAP_ARENA_SIZE_KB 30000

#define BUFFER_SIZE 1024 // TODO: make configurable
#define SAMPLE_RATE_DEFAULT 48000

#define AUDIO_FILE_CHUNK_SIZE 4096

enum  {
    ENGINE_INITIALIZED,
    ENGINE_STARTED,
    ENGINE_STOP_REQUESTED,
    ENGINE_AUDIO_THREAD_SILENCED,

    NUM_ENGINE_FLAGS,
};

typedef void (*JamProcessFunc)(f64 sampleRate, u16 numFrames, f32* buffer, void* data); 
typedef void (*JamDestroyFunc)(void* data); 

typedef struct {
    u16 inputRoutingMask, outputRoutingMask;
    void* procData;
    JamProcessFunc Process;
    JamDestroyFunc Destroy;
} JamAudioProcessor;

typedef struct {
    // Playback state
    f32 masterVolumeScale;
    f32 sampleRate; 

    // Thread messaging
    _Atomic(u8) flags;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    // Allocators
    void* heapArena;
    // TODO: bucket arena allocator
    u8 scratchArena[STACK_ARENA_SIZE_KB * 1024];
    ScratchAllocator scratchAllocator;

    // Channels
    u64 processorMask;
    u64 sourceMask;
    JamAudioProcessor processors[MAX_PROCESSORS];

    // Thread Pool
    ThreadPool threadPool;

    // CoreAudio audio unit
    AudioUnit caUnit;
    AudioStreamBasicDescription streamFormat;
} CoreEngineContext;

// Core Engine Functions
void CoreEngine_Init(CoreEngineContext* ctx, f32 masterVolumeScale, u64 heapArenaSizeKb);
void CoreEngine_Deinit(CoreEngineContext* ctx);
void CoreEngine_Start(CoreEngineContext* ctx);
void CoreEngine_Stop(CoreEngineContext* ctx);
void CoreEngine_AddSource(CoreEngineContext* ctx, u16 id);
u16 CoreEngine_CreateProcessor(CoreEngineContext* ctx, JamProcessFunc procFunc, JamDestroyFunc destFunc, void* data);
void CoreEngine_RemoveProcessor(CoreEngineContext* ctx, u16 id);
void CoreEngine_Route(CoreEngineContext* ctx, u16 inputId, u16 outputId, bool shouldRoute);
void CoreEngine_SubmitTask(CoreEngineContext* ctx, TaskInfo task);
void CoreEngine_Panic(CoreEngineContext* ctx);
void CoreEngine_GlobalPanic(void);

