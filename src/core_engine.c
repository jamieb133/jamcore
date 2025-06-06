#include <AudioToolbox/AUGraph.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudioTypes/CoreAudioBaseTypes.h>
#include <MacTypes.h>
#include <assert.h>
#include <signal.h>

#include <allocator.h>
#include <core_engine.h>
#include <logger.h>

static bool panicked_ = false;
static CoreEngineContext* currentInstance_ = NULL;

static void HandleSigInt(int sig)
{
    LogWarnRaw("\n");
    LogWarn("Caught SIGINT %d, exiting...", sig);
    CoreEngine_Panic();
}

static void RouteAudio(CoreEngineContext* context, 
                        const AudioTimeStamp* timestamp, 
                        UInt32 numFrames, 
                        float* masterBuffer)
{
    Assert(context->masterVolumeScale <= 1.0f, "Master volume scale is too large (%f)", context->masterVolumeScale);

    for (JamAudioChannelId i = 0; i < context->numChannels; i++) {
        JamAudioChannel* channel = &context->channels[i];
        struct JamAudioProcessor* node = channel->processors.head;

        while (node) {
            node->buffer = channel->scratchBuffer;
            node->numFrames = numFrames;
            node->sampleRate = context->streamFormat.mSampleRate;
            node->Process(node);
            node = node->next;
        }

        // Sum into the output buffer
        for (size_t frameIndex = 0; frameIndex < numFrames; frameIndex++) {
            UInt16 sampleIndex = frameIndex * 2;

            masterBuffer[sampleIndex] += channel->scratchBuffer[sampleIndex] * context->masterVolumeScale;
            masterBuffer[sampleIndex + 1] += channel->scratchBuffer[sampleIndex + 1] * context->masterVolumeScale;

            LogPeriodic(LOG_INFO, 500, "master buffer sample = %f", masterBuffer[sampleIndex]);
            //LogInfoPeriodic("master buffer sample = %f", 500, masterBuffer[sampleIndex + 1]);
        }
    }
}

static OSStatus AudioRenderCallback(void* args,
                                        AudioUnitRenderActionFlags* ioActionFlags,
                                        const AudioTimeStamp* timestamp,
                                        UInt32 busNumber,
                                        UInt32 numFrames,
                                        AudioBufferList *ioData) 
{
    // Unused
    (void)busNumber;

    CoreEngineContext* context = (CoreEngineContext*)args;

    // Zero out buffers
    for (UInt32 i = 0; i < ioData->mNumberBuffers; i++) {
        AudioBuffer* buffer = &ioData->mBuffers[i];
        memset(buffer->mData, 0, buffer->mDataByteSize);
    }

    float* masterBuffer = (float*)ioData->mBuffers[0].mData;
    Assert(ioData->mNumberBuffers == 1, "Error expected single interleaved audio stream");
    RouteAudio(context, timestamp, numFrames, masterBuffer);

    return noErr;
}

void CoreEngine_Init(CoreEngineContext *context, float masterVolumeScale)
{
    for (UInt16 i = 0; i < MAX_CHANNELS; i++) {
        ZeroMem(float, context->channels[i].scratchBuffer, MAX_CHANNELS);
        context->channels[i].processors.head = NULL;
        context->channels[i].processors.tail = NULL;
    }

    context->masterVolumeScale = masterVolumeScale;
}

void CoreEngine_Start(CoreEngineContext* context)
{
    OSStatus status;

    LogInfo("Initializing CoreAudio");

    context->defaultOutputDesc = (AudioComponentDescription) {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0,
    };

    context->component = AudioComponentFindNext(NULL, &context->defaultOutputDesc);
    Assert(context->component != NULL, "Failed to find default output audio component");
        
    status = AudioComponentInstanceNew(context->component, &context->unit);
    Assert(status == noErr, "Failed to instantiate audio unit. Status: %d", status);

    status = AudioUnitInitialize(context->unit);
    Assert(status == noErr, "Failed to initialize audio unit. Status: %d", status);

    // Setup single interleaved stereo stream
    context->streamFormat = (AudioStreamBasicDescription) {
        .mSampleRate = 48000,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsFloat,
        .mBytesPerFrame = sizeof(Float32) * 2,
        .mFramesPerPacket = 1,
        .mBytesPerPacket = sizeof(Float32) * 2,
        .mChannelsPerFrame = 2,
        .mBitsPerChannel = 8 * sizeof(Float32),
    };

    status = AudioUnitSetProperty(context->unit, 
                                  kAudioUnitProperty_StreamFormat, 
                                  kAudioUnitScope_Input, 
                                  0, 
                                  &context->streamFormat, 
                                  sizeof(context->streamFormat));
    Assert(status == noErr, "Could not set stream format. Status: %d", status);

    AURenderCallbackStruct inputCallback = (AURenderCallbackStruct) {
        .inputProc = AudioRenderCallback,
        .inputProcRefCon = context,
    };

    status = AudioUnitSetProperty(context->unit, 
                                  kAudioUnitProperty_SetRenderCallback, 
                                  kAudioUnitScope_Input, 
                                  0, 
                                  &inputCallback, 
                                  sizeof(inputCallback));
    Assert(status == noErr, "Could not set render callback. Status: %d", status);

    status = AudioOutputUnitStart(context->unit);
    Assert(status == noErr, "Could not start audio unit. Status: %d", status);

    // Register the SIGINT handler to gracefully close if we CTRL-C
    struct sigaction sa;
    sa.sa_handler = HandleSigInt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Must set this first before the next line in case of panic so we can close it
    currentInstance_ = context;

    Assert(sigaction(SIGINT, &sa, NULL) != -1, "Failed to register SIGINT handler");
}

void CoreEngine_Stop(CoreEngineContext* context)
{
    OSStatus status;

    LogInfo("Deinitializing CoreAudio");

    status = AudioOutputUnitStop(context->unit);
    Assert(status == noErr, "Failed to stop audio unit. Status: %d", status);

    status = AudioUnitUninitialize(context->unit);
    Assert(status == noErr, "Failed to uninitialize audio unit. Status: %d", status);

    status = AudioComponentInstanceDispose(context->unit);
    Assert(status == noErr, "Failed to dispose audio unit. Status: %d", status);

    for (UInt16 i = 0; i < context->numChannels; i++) {
        JamAudioProcessor* node = context->channels[i].processors.head;
        while (node != NULL) {
            Assert(node->Destroy, "Processor Destroy method is null");
            node->Destroy(node);
            node = node->next;
        }
    }
}

JamAudioChannelId CoreEngine_CreateChannel(CoreEngineContext *context)
{
    Assert(context->numChannels <= MAX_CHANNELS, "Reached maximum number of channels %d", MAX_CHANNELS);
    JamAudioChannelId channelId = context->numChannels;
    context->numChannels++;
    return channelId;
}

void CoreEngine_AddProcessor(CoreEngineContext *context, JamAudioChannelId channelId, JamAudioProcessor* processor)
{
    Assert(channelId < context->numChannels, "Invalid channel id: %d", channelId);
    Assert(processor, "Processor is null");

    processor->next = NULL;

    if (context->channels[channelId].processors.head == NULL) {
        context->channels[channelId].processors.head = processor;
        context->channels[channelId].processors.tail = processor;
    }
    else {
        context->channels[channelId].processors.tail->next = processor;
        context->channels[channelId].processors.tail = processor;
    }
}

void CoreEngine_Panic(void)
{   
    // Prevent recursion in case CoreEngine_Stop fails, in such case we have no option but to hard kill
    if (panicked_) {
        exit(1);
    }
    
    panicked_ = true;
    LogError("CoreEngine Panic");

    if (currentInstance_) {
        CoreEngine_Stop(currentInstance_);
    }

    // No going back!
    exit(0);
}

