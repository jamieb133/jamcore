#pragma once

#include <MacTypes.h>
#include <stdbool.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudioTypes/CoreAudioBaseTypes.h>

#define MAX_CHANNELS 1024
#define BUFFER_SIZE sizeof(Float32) // TODO: make configurable

static const UInt8 ChannelMuted = 1 << 0;
static const UInt8 ChannelSoloed = 1 << 1;
static const UInt8 ChannelMono = 1 << 2;

typedef struct {
    UInt8 options;
    double volume;
} JamAudioChannelControl;

typedef struct JamAudioProcessor {
    struct JamAudioProcessor* next;

    float* buffer;
    int numFrames;
    double sampleRate;
    void* procData; // Processor specific context

    void (*Process)(struct JamAudioProcessor* self);
    void (*Destroy)(struct JamAudioProcessor* self);
} JamAudioProcessor;

typedef struct {
    JamAudioProcessor* head;
    JamAudioProcessor* tail;
} AudioProcessorList;

typedef struct {
    AudioProcessorList processors;
    float scratchBuffer[BUFFER_SIZE];
} JamAudioChannel;

typedef UInt16 JamAudioChannelId;

typedef struct CoreEngineContext {
    // Playback State
    double startTime;
    double endTime;
    float masterVolumeScale;
    // Channels
    JamAudioChannel channels[MAX_CHANNELS];
    UInt16 numChannels;
    // CoreAudio Data
    AudioUnit unit;
    AudioComponentDescription defaultOutputDesc;
    AudioStreamBasicDescription streamFormat;
    AudioComponent component;
} CoreEngineContext;

void CoreEngine_Init(CoreEngineContext* context, float masterVolumeScale);
void CoreEngine_Start(CoreEngineContext* context);
void CoreEngine_Stop(CoreEngineContext* context);
JamAudioChannelId CoreEngine_CreateChannel(CoreEngineContext* context);
void CoreEngine_AddProcessor(CoreEngineContext* context, JamAudioChannelId channel, JamAudioProcessor* processor);
void CoreEngine_Panic(void);

