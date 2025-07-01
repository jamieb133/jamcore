#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudioTypes/CoreAudioBaseTypes.h>
#include <audio_renderer.h>
#include <stdatomic.h>

#ifndef MIN
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#endif

static void WriteNextChunk(void* data)
{
    AudioRenderer* renderer = (AudioRenderer*)data;
    Assert(renderer, "Renderer is null");

    LogInfo("Rendering %d bytes", renderer->numFramesToWrite);

    ExtAudioFileWrite(renderer->outputFile, 
                      renderer->numFramesToWrite, 
                      renderer->coreAudioBuffers[atomic_load(&renderer->currentBufferIndex) == 0 ? 1 : 0]); 

    renderer->numFramesToWrite = 0;
}

static void ScheduleWrite(AudioRenderer* renderer)
{
    u16 bufferToPassToWriteChunk = atomic_load(&renderer->currentBufferIndex);
    atomic_store(&renderer->currentBufferIndex,
                 (bufferToPassToWriteChunk == 0 ? 1 : 0));
    atomic_store(&renderer->seekPosition, 0);
    memset(renderer->coreAudioBuffers[atomic_load(&renderer->currentBufferIndex)]->mBuffers[0].mData,
           0,
           AUDIO_FILE_CHUNK_SIZE * renderer->streamFormat.mBytesPerFrame);
    ThreadPool_DeferTask(renderer->threadPool, WriteNextChunk, renderer);
}

static void ProcessRenderer(f64 sampleRate, u16 numFrames, f32* buffer, void* data)
{
    (void)sampleRate;

    AudioRenderer* renderer = (AudioRenderer*)data;
    Assert(renderer, "Renderer is null");

    if ((atomic_load(&renderer->flags) & AUDIO_RENDERER_RECORDING) == 0) {
        return;
    }

    u16 currentBufferIndex = atomic_load(&renderer->currentBufferIndex);
    u16 seekPosition = atomic_load(&renderer->seekPosition);

    f32* outputBuffer = (f32*)renderer->coreAudioBuffers[currentBufferIndex]->mBuffers[0].mData;
    u16 channelsPerFrame = renderer->streamFormat.mChannelsPerFrame;

    // TODO: how to start filling into the next buffer if it spills over?
    u16 framesToProcess = MIN(numFrames, AUDIO_FILE_CHUNK_SIZE - seekPosition);

    for (u16 i = 0; i < framesToProcess; ++i) {
        u32 outputSampleOffset = (seekPosition + i) * channelsPerFrame;
        u32 inputSampleOffset = i * 2;

        outputBuffer[outputSampleOffset] += buffer[inputSampleOffset];     // Left channel
        outputBuffer[outputSampleOffset + 1] += buffer[inputSampleOffset + 1]; // Right channel

        if (atomic_load(&renderer->flags) & AUDIO_RENDERER_MUTE) {
            outputBuffer[outputSampleOffset] = 0.0f;
            outputBuffer[outputSampleOffset + 1] = 0.0f;
        }
    }

    renderer->framesThisCycle = numFrames;
}

static void DestroyRenderer(void* data)
{
    AudioRenderer* renderer = (AudioRenderer*)data;
    Assert(renderer, "Renderer is null");

    // If the renderer is still running on shutdown, flush the remaining data
    if ((renderer->flags & AUDIO_RENDERER_RECORDING) && (renderer->seekPosition > 0)) {
        renderer->numFramesToWrite = renderer->seekPosition;
        renderer->currentBufferIndex = (renderer->currentBufferIndex == 0) ? 1 : 0;
        WriteNextChunk((void*)renderer);    
    }

    renderer->flags &= ~AUDIO_RENDERER_RECORDING;

    LogInfo("Destroying AudioRenderer");

    for (u8 i = 0; i < 2; i++) {
        free(renderer->coreAudioBuffers[i]->mBuffers[0].mData);
        free(renderer->coreAudioBuffers[i]);
    }

    ExtAudioFileDispose(renderer->outputFile);
}

static void RendererOnNewAudioCycle(void* data)
{
    AudioRenderer* renderer = (AudioRenderer*)data;
    Assert(renderer, "Renderer is null");

    u16 oldSeekPosition = atomic_fetch_add(&renderer->seekPosition, renderer->framesThisCycle);
    u16 newCalculatedSeekPosition = oldSeekPosition + renderer->framesThisCycle; // This is the logical end position

    if (newCalculatedSeekPosition >= AUDIO_FILE_CHUNK_SIZE) {
        atomic_store(&renderer->numFramesToWrite, AUDIO_FILE_CHUNK_SIZE);
        LogInfo("Scheduling Write (Full Chunk, previous seek %u, frames processed %u)", oldSeekPosition, renderer->framesThisCycle);
        ScheduleWrite(renderer);
    }
}

u16 AudioRenderer_Create(AudioRenderer* renderer, CoreEngineContext* ctx, const char* filename)
{
    Assert(renderer, "Renderer is null");
    Assert(ctx, "Engine is null");

    LogInfo("Creating AudioRenderer for %s", filename);

    OSStatus status;

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL/*TODO: use custom heap*/,
        (const u8*)filename,
        strlen(filename),
        false // IsDirectory
    );

    CFShow(url);

    memset(&renderer->streamFormat, 0, sizeof(AudioStreamBasicDescription));
    renderer->streamFormat = (AudioStreamBasicDescription) {
        .mSampleRate = SAMPLE_RATE_DEFAULT,
        .mFormatID = kAudioFormatLinearPCM,
        .mBytesPerPacket = 4 * 2,
        .mFramesPerPacket = 1,
        .mBytesPerFrame = 4 * 2,
        .mChannelsPerFrame = 2,
        .mBitsPerChannel = 32,
        .mFormatFlags = kLinearPCMFormatFlagIsFloat |
                        kLinearPCMFormatFlagIsPacked,
    };

    status = ExtAudioFileCreateWithURL(url, 
                                       kAudioFileWAVEType, 
                                       &renderer->streamFormat, 
                                       NULL, // Default channel layout
                                       kAudioFileFlags_EraseFile,
                                       &renderer->outputFile);
    Assert(status == noErr, "Failed to create output file for renderer %s, %d", filename, status);

    status = ExtAudioFileSetProperty(renderer->outputFile,
                                     kExtAudioFileProperty_ClientDataFormat,
                                     sizeof(renderer->streamFormat),
                                     &renderer->streamFormat);
    Assert(status == noErr, "Failed to set data format for renderer %s, %d", filename, status);

    u32 dataByteSize = AUDIO_FILE_CHUNK_SIZE * renderer->streamFormat.mBytesPerFrame;
    for (u8 i = 0; i < 2; i++) {
        renderer->coreAudioBuffers[i] = malloc(sizeof(AudioBufferList) + (sizeof(AudioBuffer)));
        renderer->coreAudioBuffers[i]->mNumberBuffers = 1;
        renderer->coreAudioBuffers[i]->mBuffers[0].mNumberChannels = 2;
        renderer->coreAudioBuffers[i]->mBuffers[0].mDataByteSize = dataByteSize;
        renderer->coreAudioBuffers[i]->mBuffers[0].mData = malloc(dataByteSize);
    }

    renderer->currentBufferIndex = 0;
    renderer->flags = 0;
    renderer->seekPosition = 0;
    renderer->framesThisCycle = 0;
    renderer->threadPool = &ctx->threadPool;

    CFRelease(url);

    return CoreEngine_CreateProcessor(ctx, ProcessRenderer, DestroyRenderer, RendererOnNewAudioCycle, (void*)renderer);
}

void AudioRenderer_StartRecord(AudioRenderer* renderer)
{
    renderer->flags |= AUDIO_RENDERER_RECORDING;
}

void AudioRenderer_StopRecord(AudioRenderer* renderer)
{
    renderer->flags &= ~AUDIO_RENDERER_RECORDING;
    
    if (atomic_load(&renderer->seekPosition) > 0) {
        renderer->numFramesToWrite = renderer->seekPosition;
        ScheduleWrite(renderer);
    }
}

