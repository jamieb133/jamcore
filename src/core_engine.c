#include <AudioToolbox/AUGraph.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudioTypes/CoreAudioBaseTypes.h>
#include <MacTypes.h>
#include <pthread.h>
#include <signal.h>

#include <allocator.h>
#include <core_engine.h>
#include <logger.h>
#include <stdint.h>
#include <utils.h>

static CoreEngineContext* instance_ = NULL;

static void HandleSigInt(int sig)
{
    LogWarnRaw("\n");
    LogWarn("Caught SIGINT %d, exiting...", sig);
    Assert(false, "Triggering panic");
}

static void AssertHandler(const char* message, const char* file, i32 line)
{
    fprintf(stderr, "%s %s:%d\n", message, file, line);
    CoreEngine_GlobalPanic();
}

static inline void SetFlag(CoreEngineContext* ctx, u8 flag)
{
    ctx->flags |= (1 << flag);
}

static inline void UnsetFlag(CoreEngineContext* ctx, u8 flag)
{
    ctx->flags &= ~(1 << flag);
}

static inline bool IsFlagSet(CoreEngineContext* ctx, u8 flag)
{
    return ctx->flags & (1 << flag);
}

static void Traverse(JamAudioProcessor* processors, 
                        u16 processorId, 
                        f64 sampleRate, 
                        u16 numFrames, 
                        f32* inputBuffer, 
                        f32* outputBuffer, 
                        u8 stackDepth, 
                        ScratchAllocator* alloc)
{
    Assert(stackDepth < 128, "Stack depth limit exceeded");

    // Do DSP
    JamAudioProcessor* processor = &processors[processorId];
    processor->Process(sampleRate, numFrames, inputBuffer, processor->procData);

    // End of branch, write to master buffer then exit
    if (processor->outputRoutingMask == 0) {
        BufferParallelSum(outputBuffer, inputBuffer, BUFFER_SIZE);
        return;
    }

    // Traverse any children
    u16 currentMask = processor->outputRoutingMask;
    while (currentMask != 0) {
        u16 nextId = CountTrailingZeros(currentMask);
        currentMask &= ~(1 << nextId); 
        f32* nextBuffer = ScratchAllocator_Alloc(alloc, BUFFER_SIZE * sizeof(f32));
        memcpy(nextBuffer, inputBuffer, BUFFER_SIZE * sizeof(f32));
        Traverse(processors, nextId, sampleRate, numFrames, nextBuffer, outputBuffer, stackDepth + 1, alloc);
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
    (void)timestamp; // TODO: probably want to use this
    (void)busNumber;
    (void)ioActionFlags;

    CoreEngineContext* ctx = (CoreEngineContext*)args;
    AudioBuffer* masterBuffer = &ioData->mBuffers[0];

    // ========================================================================
    // Bail out immediatelly after zeroing buffer if engine has stopped
    // ========================================================================

    for (UInt32 i = 0; i < ioData->mNumberBuffers; i++) {
        AudioBuffer* buffer = &ioData->mBuffers[i];
        memset(buffer->mData, 0, buffer->mDataByteSize);
    }

    Assert(numFrames <= BUFFER_SIZE, "Number of frames exceeds buffer capacity");

    // Note: this must come after the buffers are zeroed out above to prevent horrible glitching!
    if (!IsFlagSet(ctx, ENGINE_STARTED)) {
        return noErr;
    }

    // ========================================================================
    // If fading out then adjust the master volume accordingly
    // ========================================================================

    if (IsFlagSet(ctx, ENGINE_STOP_REQUESTED)) {
        // TODO: actually fade out based on time
        ctx->masterVolumeScale = 0.0;
        
        // Signal the main thread to continue
        SetFlag(ctx, ENGINE_AUDIO_THREAD_SILENCED);
        Assert(pthread_cond_signal(&ctx->cond) == 0, "Failed to signal condition variable");

        return noErr;
    }

    Assert(ioData->mNumberBuffers == 1, "Error expected single interleaved audio stream");

    // ========================================================================
    // Traverse the audio graph
    // ========================================================================

    if ((ctx->processorMask & (1 << ctx->sourceNode)) == 0) {
        return noErr;
    }

    f32* inputBuffer = ScratchAllocator_Calloc(&ctx->scratchAllocator, BUFFER_SIZE * sizeof(f32));

    Traverse(
        ctx->processors, 
        ctx->sourceNode, 
        ctx->sampleRate, 
        numFrames, 
        inputBuffer, 
        masterBuffer->mData,
        0, // Stack depth
        &ctx->scratchAllocator
    );

    BufferProduct(masterBuffer->mData, ctx->masterVolumeScale, BUFFER_SIZE);
    ScratchAllocator_Release(&ctx->scratchAllocator);

    return noErr;
}

void CoreEngine_Init(CoreEngineContext *ctx, float masterVolumeScale, u64 heapArenaSizeKb)
{
    RegisterAssertHandler(AssertHandler);

    Assert(ctx, "Context is null");
    Assert(heapArenaSizeKb > 0, "Provided heap size must be greater than zero");
    Assert(instance_ == NULL, "Error, already existing instance of engine");

    ctx->heapArena = malloc(heapArenaSizeKb * 1024); 
    Assert(ctx->heapArena, "Failed to allocate %d kilobytes for heap arena");

    memset(ctx->scratchArena, 0, STACK_ARENA_SIZE_KB * 1024);
    ScratchAllocator_Init(&ctx->scratchAllocator, ctx->scratchArena, STACK_ARENA_SIZE_KB * 1024);

    Assert(pthread_mutex_init(&ctx->mutex, NULL) == 0, "Failed to initialise mutex");
    Assert(pthread_cond_init(&ctx->cond, NULL) == 0, "Failed to initialise condition variable");

    memset(ctx->processors, 0, MAX_PROCESSORS);

    ctx->masterVolumeScale = masterVolumeScale;
    ctx->processorMask = 0;
    ctx->sampleRate = 0;

    instance_ = ctx;
    SetFlag(ctx, ENGINE_INITIALIZED);
}

void CoreEngine_Deinit(CoreEngineContext* ctx)
{
    Assert(IsFlagSet(ctx, ENGINE_INITIALIZED), "Engine not intialised");
    Assert(!IsFlagSet(ctx, ENGINE_STARTED), "Engine is running on deinit, must stop it first");

    UInt16 numProcessors = Bitcount(ctx->processorMask);
    for (UInt16 i = 0; i < MAX_PROCESSORS; i++) {
        if (numProcessors == 0)
            break;
        if (ctx->processorMask & (1 << i)) {
            JamAudioProcessor* proc = &ctx->processors[i];
            if (proc->Destroy) {
                proc->Destroy(proc->procData);
            }
            numProcessors--;
        }
    }

    Assert(pthread_mutex_destroy(&ctx->mutex) == 0, "Failed to destroy mutex");
    Assert(pthread_cond_destroy(&ctx->cond) == 0, "Failed to destroy condition variable");

    ctx->flags = 0;
    ScratchAllocator_Release(&ctx->scratchAllocator);
    instance_ = NULL;
}

void CoreEngine_Start(CoreEngineContext* ctx)
{
    OSStatus status;
   
    Assert(instance_, "Instance is null, may not have been initialised");
    LogInfo("Initializing CoreAudio");

    AudioComponentDescription defaultOutputDesc = (AudioComponentDescription) {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0,
    };

    AudioComponent component = AudioComponentFindNext(NULL, &defaultOutputDesc);
    Assert(component != NULL, "Failed to find default output audio component");
        
    status = AudioComponentInstanceNew(component, &ctx->caUnit);
    Assert(status == noErr, "Failed to instantiate audio unit. Status: %d", status);

    status = AudioUnitInitialize(ctx->caUnit);
    Assert(status == noErr, "Failed to initialize audio unit. Status: %d", status);

    // Setup single interleaved stereo stream
    AudioStreamBasicDescription streamFormat = (AudioStreamBasicDescription) {
        .mSampleRate = SAMPLE_RATE_DEFAULT, // TODO: make configurable
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsFloat,
        .mBytesPerFrame = sizeof(Float32) * 2,
        .mFramesPerPacket = 1,
        .mBytesPerPacket = sizeof(Float32) * 2,
        .mChannelsPerFrame = 2,
        .mBitsPerChannel = 8 * sizeof(Float32),
    };

    status = AudioUnitSetProperty(ctx->caUnit, 
                                  kAudioUnitProperty_StreamFormat, 
                                  kAudioUnitScope_Input, 
                                  0, // Global scope
                                  &streamFormat, 
                                  sizeof(streamFormat));
    Assert(status == noErr, "Could not set stream format. Status: %d", status);

    ctx->sampleRate = (f32)streamFormat.mSampleRate;

    const UInt32 maxFrames = 1024;
    status = AudioUnitSetProperty(
        ctx->caUnit,
        kAudioUnitProperty_MaximumFramesPerSlice,
        kAudioUnitScope_Global,
        0, // Global scope
        &maxFrames,
        sizeof(maxFrames)
    );

    Assert(status == noErr, "Failed to set desired buffer size. Status: %d", status);

    AURenderCallbackStruct inputCallback = (AURenderCallbackStruct) {
        .inputProc = AudioRenderCallback,
        .inputProcRefCon = ctx,
    };

    status = AudioUnitSetProperty(ctx->caUnit, 
                                  kAudioUnitProperty_SetRenderCallback, 
                                  kAudioUnitScope_Input, 
                                  0, // Global scope
                                  &inputCallback, 
                                  sizeof(inputCallback));
    Assert(status == noErr, "Could not set render callback. Status: %d", status);

    status = AudioOutputUnitStart(ctx->caUnit);
    Assert(status == noErr, "Could not start audio unit. Status: %d", status);

    // Register the SIGINT handler to gracefully close if we CTRL-C
    struct sigaction sa;
    sa.sa_handler = HandleSigInt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Must set this first before the next line in case of panic so we can close it
    instance_ = ctx;

    Assert(sigaction(SIGINT, &sa, NULL) != -1, "Failed to register SIGINT handler");
    SetFlag(ctx, ENGINE_STARTED);
}

void CoreEngine_Stop(CoreEngineContext* ctx)
{
    Assert(IsFlagSet(ctx, ENGINE_INITIALIZED), "Engine not initialised");
    Assert(IsFlagSet(ctx, ENGINE_STARTED), "Engine not started");

    OSStatus status;
    SetFlag(ctx, ENGINE_STOP_REQUESTED);

    LogInfo("Deinitializing CoreAudio");

    // Wait for fade out
    Assert(pthread_mutex_lock(&ctx->mutex) == 0, "Failed to lock mutex");
    Assert(pthread_cond_wait(&ctx->cond, &ctx->mutex) == 0, "Failed to wait for condition variable");
    Assert(pthread_mutex_unlock(&ctx->mutex) == 0, "Failed to unlock mutex");
    UnsetFlag(ctx, ENGINE_STARTED);

    status = AudioOutputUnitStop(ctx->caUnit);
    Assert(status == noErr, "Failed to stop audio unit. Status: %d", status);

    status = AudioUnitUninitialize(ctx->caUnit);
    Assert(status == noErr, "Failed to uninitialize audio unit. Status: %d", status);

    status = AudioComponentInstanceDispose(ctx->caUnit);
    Assert(status == noErr, "Failed to dispose audio unit. Status: %d", status);
}

void CoreEngine_SetSource(CoreEngineContext* ctx, u16 id)
{
    Assert(ctx, "Context is null");
    Assert(IsFlagSet(ctx, ENGINE_INITIALIZED), "Engine not initialised");
    Assert(id < MAX_PROCESSORS, "Invalid processor id");
    ctx->sourceNode = id;
}

u16 CoreEngine_CreateProcessor(CoreEngineContext* ctx, JamProcessFunc procFunc, JamDestroyFunc destFunc, void* data)
{
    Assert(ctx, "Context is null");
    Assert(IsFlagSet(ctx, ENGINE_INITIALIZED), "Engine not initialised");
    // TODO assert capacity
    
    // Find free slot, the first trailing zero in the mask
    UInt16 freeSlot = (ctx->processorMask == 0) ? 0 : __builtin_ctz((unsigned int)~ctx->processorMask);
    ctx->processorMask |= (1 << freeSlot);

    JamAudioProcessor* processor = &ctx->processors[freeSlot];
    processor->inputRoutingMask = 0;
    processor->outputRoutingMask = 0;
    processor->Process = procFunc;
    processor->Destroy = destFunc;
    processor->procData = data;

    return freeSlot;
}

void CoreEngine_RemoveProcessor(CoreEngineContext *ctx, u16 id)
{
    Assert(ctx, "Context is null");
    Assert(id < MAX_PROCESSORS, "Invalid processor id %d", id);
    Assert(ctx->processorMask & (1 << id), "Tried to remove non-existing processor %d", id);

    ctx->processorMask &= ~(1 << id);
}

void CoreEngine_Route(CoreEngineContext *ctx, u16 srcId, u16 dstId, bool shouldRoute)
{
    Assert(ctx, "Context is null");
    Assert(srcId < MAX_PROCESSORS, "Invalid src processor id %d", srcId);
    Assert(dstId < MAX_PROCESSORS, "Invalid dst processor id %d", dstId);
    Assert(ctx->processorMask & (1 << srcId), "Tried to route non-existing src processor %d", srcId);
    Assert(ctx->processorMask & (1 << dstId), "Tried to route non-existing dst processor %d", dstId);

    JamAudioProcessor* srcProc = &ctx->processors[srcId];
    JamAudioProcessor* dstProc = &ctx->processors[dstId];

    if (shouldRoute) {
        srcProc->outputRoutingMask |= (1 << dstId);
        dstProc->inputRoutingMask |= (1 << srcId);
    }
    else {
        srcProc->outputRoutingMask &= ~(1 << dstId);
        dstProc->inputRoutingMask &= ~(1 << srcId);
    }
}

void CoreEngine_Panic(CoreEngineContext* ctx)
{   
    static u8 numPanics = 0;
    LogError("CoreEngine Panic %d", numPanics);

    instance_ = NULL;

    if (ctx == NULL) {
        exit(0);
    }

    // Prevent recursion in case CoreEngine_Stop fails, in such case we have no option but to hard kill
    if (IsFlagSet(ctx, ENGINE_STOP_REQUESTED)) {
        LogWarn("CoreEngine error occurred while trying to stop after previous panic");
        exit(1);
    }
    
    // No going back!
    CoreEngine_Stop(ctx);
    exit(0);
}

void CoreEngine_GlobalPanic(void)
{
    if (instance_ == NULL) {
        exit(1);
    }
    CoreEngine_Panic(instance_);
}   

