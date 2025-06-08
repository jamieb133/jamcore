#include <AudioToolbox/AUGraph.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudioTypes/CoreAudioBaseTypes.h>
#include <MacTypes.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>

#include <allocator.h>
#include <core_engine.h>
#include <logger.h>
#include <stdint.h>

static _Atomic(bool) stopped_ = false;
static CoreEngineContext* currentInstance_ = NULL;

static void HandleSigInt(int sig)
{
    LogWarnRaw("\n");
    LogWarn("Caught SIGINT %d, exiting...", sig);
    CoreEngine_Panic();
}

static UInt16 GetScratchBuffers(JamAudioChannel* allChannels, UInt16 channelMask, float** buffers)
{
    Assert(buffers, "Buffers is null");

    UInt16 numFound = 0;
    UInt16 numChannels = __builtin_popcount(channelMask);

    for (UInt16 i = 0; i < MAX_CHANNELS; i++) {
        if (numFound == numChannels) {
            return numFound;
        }

        if (channelMask & (1 << i)) {
            buffers[numFound] = allChannels[i].scratchBuffer;
            numFound++;
        }
    }

    Assert(false, "Failed to find all scratch buffers");

    // Will not get here
    return 0;
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

    // ========================================================================
    // Bail out immediatelly after zeroing buffer if engine has stopped
    // ========================================================================
    
    for (UInt32 i = 0; i < ioData->mNumberBuffers; i++) {
        AudioBuffer* buffer = &ioData->mBuffers[i];
        memset(buffer->mData, 0, buffer->mDataByteSize);
    }

    Assert(numFrames <= BUFFER_SIZE, "Number of frames exceeds channel buffer capacity");

    // Note: this must come after the buffers are zeroed out above to prevent horrible glitching!
    if (context->flags & ENGINE_STOPPED) {
        return noErr;
    }

    // ========================================================================
    // If fading out then adjust the master volume accordingly
    // ========================================================================

    if (context->flags & ENGINE_STOP_REQUESTED) {
        // TODO: actually fade out based on time
        context->masterVolumeScale = 0.0;
        
        // Signal the main thread to continue
        context->flags |= ENGINE_STOPPED;
        Assert(pthread_cond_signal(&context->cond) == 0, "Failed to signal condition variable");

        return noErr;
    }

    Assert(ioData->mNumberBuffers == 1, "Error expected single interleaved audio stream");

    // ========================================================================
    // Run all processors with their channel buffers
    // ========================================================================

    UInt16 numProcessors = __builtin_popcount(context->processorMask);

    for (UInt16 i = 0; i < MAX_PROCESSORS; i++) { 
        if (numProcessors == 0) {
            break;
        }

        if (context->processorMask & (1 << i)) {
            JamAudioProcessor* processor = &context->processors[i];
            float* scratchBuffers[MAX_CHANNELS] = { NULL, };
            UInt16 numChannels = GetScratchBuffers(context->channels, processor->channelMask, scratchBuffers);

            LogInfoOnce("Num scratch buffers %d", numChannels);

            // Only process audio if there are any channels
            if (numChannels > 0) {
                processor->Process(processor, numFrames, context->streamFormat.mSampleRate, scratchBuffers, numChannels);
            }

            numProcessors--;
        }
    }

    Assert(numProcessors == 0, "Did not route all processors");

    // ========================================================================
    // Sum all channel audio into the master mix
    // ========================================================================

    f32* masterBuffer = (float*)ioData->mBuffers[0].mData;
    u16 numChannels = __builtin_popcount(context->channelMask);

    for (u16 i = 0; i < MAX_CHANNELS; i++) {
        if (numChannels == 0) {
            break;
        }

        if (context->channelMask & (1 << i)) {
            JamAudioChannel* chan = &context->channels[i];
            f32 chanVol = ((f32)chan->volume) / 255;

            Assert(chanVol <= 1.0f, "Channel volume too large %f", chanVol);

            f32 normalisedPan = (f32)chan->pan / (f32)UINT8_MAX;
            f32 gainLeft = cosf(normalisedPan * (M_PI / 2.0f));
            f32 gainRight = sinf(normalisedPan * (M_PI / 2.0f));

            for (u16 frame = 0; frame < numFrames; frame++) {
                f32 sampleLeft, sampleRight;
                u16 sampleIndex = frame * 2;

                sampleLeft = chan->scratchBuffer[sampleIndex];
                sampleRight = chan->scratchBuffer[sampleIndex + 1];

                sampleLeft *= chanVol;
                sampleRight *= chanVol;
                
                sampleLeft *= gainLeft;
                sampleRight *= gainRight;

                masterBuffer[sampleIndex] += sampleLeft;
                masterBuffer[sampleIndex + 1] += sampleRight;

                LogPeriodic(LOG_INFO, 500, "master buffer samples = (%f, %f)", masterBuffer[sampleIndex], masterBuffer[sampleIndex + 1]);
            }

            numChannels--;
        }
    }

    Assert(numChannels == 0, "Failed to sum all channels into master mix");

    return noErr;
}

void CoreEngine_Init(CoreEngineContext *context, float masterVolumeScale)
{
    Assert(pthread_mutex_init(&context->mutex, NULL) == 0, "Failed to initialise mutex");
    Assert(pthread_cond_init(&context->cond, NULL) == 0, "Failed to initialise condition variable");

    memset(context->channels, 0, MAX_CHANNELS);
    memset(context->processors, 0, MAX_PROCESSORS);

    context->masterVolumeScale = masterVolumeScale;
    context->channelMask = 0;
    context->processorMask = 0;
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
                                  0, // Global scope
                                  &context->streamFormat, 
                                  sizeof(context->streamFormat));
    Assert(status == noErr, "Could not set stream format. Status: %d", status);

    const UInt32 maxFrames = 1024;
    status = AudioUnitSetProperty(
        context->unit,
        kAudioUnitProperty_MaximumFramesPerSlice,
        kAudioUnitScope_Global,
        0, // Global scope
        &maxFrames,
        sizeof(maxFrames)
    );

    Assert(status == noErr, "Failed to set desired buffer size. Status: %d", status);

    AURenderCallbackStruct inputCallback = (AURenderCallbackStruct) {
        .inputProc = AudioRenderCallback,
        .inputProcRefCon = context,
    };

    status = AudioUnitSetProperty(context->unit, 
                                  kAudioUnitProperty_SetRenderCallback, 
                                  kAudioUnitScope_Input, 
                                  0, // Global scope
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
    stopped_ = true; // Required to prevent recursion on pacic

    LogInfo("Deinitializing CoreAudio");

    // Request stop and wait for fade out
    context->flags |= ENGINE_STOP_REQUESTED;
    Assert(pthread_mutex_lock(&context->mutex) == 0, "Failed to lock mutex");
    Assert(pthread_cond_wait(&context->cond, &context->mutex) == 0, "Failed to wait for condition variable");
    Assert(pthread_mutex_unlock(&context->mutex) == 0, "Failed to unlock mutex");

    status = AudioOutputUnitStop(context->unit);
    Assert(status == noErr, "Failed to stop audio unit. Status: %d", status);

    status = AudioUnitUninitialize(context->unit);
    Assert(status == noErr, "Failed to uninitialize audio unit. Status: %d", status);

    status = AudioComponentInstanceDispose(context->unit);
    Assert(status == noErr, "Failed to dispose audio unit. Status: %d", status);

    UInt16 numProcessors = __builtin_popcount(context->processorMask);
    for (UInt16 i = 0; i < MAX_PROCESSORS; i++) {
        if (numProcessors == 0)
            break;
        if (context->processorMask & (1 << i)) {
            JamAudioProcessor* proc = &context->processors[i];
            if (proc->Destroy) {
                proc->Destroy(proc);
            }
            numProcessors--;
        }
    }

    Assert(pthread_mutex_destroy(&context->mutex) == 0, "Failed to destroy mutex");
    Assert(pthread_cond_destroy(&context->cond) == 0, "Failed to destroy condition variable");
}

void CoreEngine_EnableChannel(CoreEngineContext *context, UInt16 channelId, bool enable)
{
    Assert(context, "Context is null");
    Assert(channelId < MAX_CHANNELS, "Channel ID invalid");

    if (enable) {
        context->channelMask |= (1 << channelId);
    }
    else {
        context->channelMask &= ~(1 << channelId);
    }
}

JamAudioProcessor* CoreEngine_CreateProcessor(CoreEngineContext* context, JamProcessFunc process, JamDestroyFunc destroy, void* args)
{
    Assert(context, "Context is null");
    // TODO assert capacity
    
    // Find free slot, the first trailing zero in the mask
    UInt16 freeSlot = (context->processorMask == 0) ? 0 : __builtin_ctz((unsigned int)~context->processorMask);
    context->processorMask |= (1 << freeSlot);

    JamAudioProcessor* processor = &context->processors[freeSlot];
    processor->Process = process;
    processor->Destroy = destroy;
    processor->procData = args;
    processor->channelMask = 0;

    return processor;
}

void CoreEngine_Panic(void)
{   
    // Prevent recursion in case CoreEngine_Stop fails, in such case we have no option but to hard kill
    if (stopped_) {
        exit(1);
    }
    
    LogError("CoreEngine Panic");

    if (currentInstance_) {
        CoreEngine_Stop(currentInstance_);
    }

    // No going back!
    exit(0);
}


