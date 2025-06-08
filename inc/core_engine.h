#pragma once

#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <MacTypes.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudioTypes/CoreAudioBaseTypes.h>
#include <stdint.h>

#define MAX_CHANNELS 1024
#define MAX_PROCESSORS 256

#define BUFFER_SIZE 1024 // TODO: make configurable

#define CHANNEL_SOLO (1 << 0)
#define CHANNEL_MONO (1 << 1)

#define ENGINE_STOPPED (1 << 0)
#define ENGINE_STOP_REQUESTED (1 << 1)

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef struct JamAudioProcessor {
    _Atomic(u16) channelMask;
    void* procData;
    void (*Process)(struct JamAudioProcessor* self, int numFrames, double sampleRate, float** buffers, UInt16 numBuffers);
    void (*Destroy)(struct JamAudioProcessor* self);
} JamAudioProcessor;

typedef void (*JamProcessFunc)(struct JamAudioProcessor*, int, double, float**, UInt16); 
typedef void (*JamDestroyFunc)(struct JamAudioProcessor*); 

typedef struct {
    _Atomic(u8) options;
    _Atomic(u8) volume;
    _Atomic(u8) pan; 
    float scratchBuffer[BUFFER_SIZE];
} JamAudioChannel;

typedef struct {
    // Playback State
    f32 masterVolumeScale;

    // Thread messaging
    _Atomic(u8) flags;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    // Channels
    JamAudioChannel channels[MAX_CHANNELS];
    _Atomic(u16) channelMask;

    // Processors
    // TODO: processor local memory pool, bump allocate processor specific context
    // UInt8 mempool[MAX_PROCESSORS * MAX_LOCAL_MEM];
    JamAudioProcessor processors[MAX_PROCESSORS];
    u16 processorMask;

    // CoreAudio Data
    AudioUnit unit;
    AudioComponentDescription defaultOutputDesc;
    AudioStreamBasicDescription streamFormat;
    AudioComponent component;
} CoreEngineContext;

// Core Engine Functions
void CoreEngine_Init(CoreEngineContext* context, f32 masterVolumeScale);
void CoreEngine_Start(CoreEngineContext* context);
void CoreEngine_Stop(CoreEngineContext* context);
void CoreEngine_EnableChannel(CoreEngineContext* context, u16 channelId, bool enable);
JamAudioProcessor* CoreEngine_CreateProcessor(CoreEngineContext* context, 
                                                JamProcessFunc process, 
                                                JamDestroyFunc destroy, 
                                                void* args);
void CoreEngine_Panic(void);

// Processor Functions
static inline void Processor_RouteChannel(JamAudioProcessor* processor, u16 channelId, bool enable)
{
    if (enable) {
        processor->channelMask |= (1 << channelId);
    }
    else {
        processor->channelMask &= ~(1 << channelId);
    }
}

