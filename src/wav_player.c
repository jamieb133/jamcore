#include "logger.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudioTypes/CoreAudioBaseTypes.h>
#include <CoreServices/CoreServices.h>
#include <MacTypes.h>
#include <stdatomic.h>
#include <string.h>
#include <wav_player.h>

static u32 numWavPlayers_ = 0;

static void LoadNextChunk(void* data)
{
    WavPlayer* player = (WavPlayer*)data;
    Assert(player != NULL, "WavPlayer is NULL");

    OSStatus status;
    u32 framesToRead = AUDIO_FILE_CHUNK_SIZE;
    u8 bufferToLoad = (atomic_load(&player->currentBufferIndex) == 0) ? 1 : 0;

    if (player->flags & WAVPLAYER_RESET_CURSOR) {
        status = ExtAudioFileSeek(player->audioFile, 0);
        Assert(status == noErr, "Failed to reset wav file cursor to start for id %d", player->id);
        atomic_fetch_and(&player->flags, ~WAVPLAYER_RESET_CURSOR);
        atomic_store(&player->currentFrame, 0);
    }

    status = ExtAudioFileRead(player->audioFile, &framesToRead, player->coreAudioBuffers[bufferToLoad]);
    Assert(status == noErr, "Failed to load audio file for WavPlayer %d", player->id);

    atomic_store(&player->currentNumFrames[bufferToLoad], framesToRead);

    if ((framesToRead == 0) && ((player->flags & WAVPLAYER_LOOPING) == 0))  {
        atomic_fetch_or(&player->flags, WAVPLAYER_FINISHED);
    }
}

static void ProcessWavPlayer(f64 sampleRate, u16 numOutputFrames, f32* buffer, void* data)
{
    (void)sampleRate;

    WavPlayer* player = (WavPlayer*)data;
    Assert(player, "WavPlayer is null");

    if (player->flags & WAVPLAYER_FINISHED) {
        return;
    }

    // Cache atomics
    u8 currentBufferIndex = atomic_load(&player->currentBufferIndex);
    u64 currentFrame = atomic_load(&player->currentFrame);
    u64 currentNumFrames = atomic_load(&player->currentNumFrames[currentBufferIndex]); 

    u64 currentFrameInBuffer = currentFrame % AUDIO_FILE_CHUNK_SIZE;
    u64 remainingFrames = currentNumFrames - currentFrameInBuffer;
    u16 framesThisTime = (remainingFrames < numOutputFrames) ? remainingFrames : numOutputFrames;
    f32* wavBuffer = (f32*)player->coreAudioBuffers[currentBufferIndex]->mBuffers[0].mData;
    u32 baseSampleIndex = currentFrameInBuffer * 2;

    // Copy audio data from the Wav file into the output buffer
    for (u16 i = 0; i < framesThisTime; i++) {
        u32 sampleIndex = baseSampleIndex + i * 2;
        buffer[i * 2] += wavBuffer[sampleIndex];
        buffer[i * 2 + 1] += wavBuffer[sampleIndex + 1];
    }

    atomic_fetch_add(&player->currentFrame, framesThisTime);
    currentFrameInBuffer += framesThisTime;

    // Finished streaming the current buffer?
    if (currentFrameInBuffer >= currentNumFrames) {
        if (currentFrame < (player->totalFrames - 1)) {
            // File isn't finished, load the next chunk
            atomic_store(&player->currentBufferIndex, (currentBufferIndex == 0) ? 1 : 0);
            ThreadPool_DeferTask(player->threadPool, LoadNextChunk, (void*)player);
        }
        else if (atomic_load(&player->flags) & WAVPLAYER_LOOPING) {
            // File is finished but should loop, reset the cursor and load the next chunk
            atomic_fetch_or(&player->flags, WAVPLAYER_RESET_CURSOR);
            atomic_store(&player->currentBufferIndex, (currentBufferIndex == 0) ? 1 : 0);
            ThreadPool_DeferTask(player->threadPool, LoadNextChunk, (void*)player);
        }
        else {
            // File is finished but not looping, just mark as finished
            atomic_fetch_or(&player->flags, WAVPLAYER_FINISHED);
        }
    }
}

static void DestroyWavPlayer(void* data)
{
    WavPlayer* player = (WavPlayer*)data;
    Assert(player, "WavPlayer is null");
    LogInfo("Destroying WavPlayer");
    for (u8 i = 0; i < 2; i++) {
        free(player->coreAudioBuffers[i]->mBuffers[0].mData);
        free(player->coreAudioBuffers[i]);
    }
    ExtAudioFileDispose(player->audioFile);
}

u16 WavPlayer_Create(WavPlayer* player, CoreEngineContext* ctx, const char* filename, u8 flags)
{
    Assert(player != NULL, "WavPlayer is NULL");
    Assert(ctx != NULL, "CoreEngineContext is NULL");

    LogInfo("Creating WavPlayer { id: %d, file: %s }", numWavPlayers_, filename);

    OSStatus status;

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL/*TODO: use custom heap*/,
        (const u8*)filename,
        strlen(filename),
        false // IsDirectory
    );

    status = ExtAudioFileOpenURL(url, &player->audioFile);
    Assert(status == noErr, "Failed to open %s", filename);

    AudioStreamBasicDescription streamFormat = (AudioStreamBasicDescription) {
        .mSampleRate = 44100,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
        .mBytesPerPacket = 4 * 2,
        .mFramesPerPacket = 1,
        .mBytesPerFrame = 4 * 2,
        .mChannelsPerFrame = 2,
        .mBitsPerChannel = 32,
    };

    status = ExtAudioFileSetProperty(player->audioFile, kExtAudioFileProperty_ClientDataFormat, sizeof(streamFormat), &streamFormat);
    Assert(status == noErr, "Failed to set file properties for %s", filename);

    i64 totalFrames;
    u32 propSize = sizeof(totalFrames);
    status = ExtAudioFileGetProperty(player->audioFile, kExtAudioFileProperty_FileLengthFrames, &propSize, &totalFrames);
    Assert(status == noErr, "Failed to get file properties for %s", filename);
    Assert(totalFrames > 0, "Total frames read in %s was 0", filename);

    u32 dataByteSize = AUDIO_FILE_CHUNK_SIZE * streamFormat.mBytesPerFrame;
    for (u8 i = 0; i < 2; i++) {
        player->coreAudioBuffers[i] = malloc(sizeof(AudioBufferList) + (sizeof(AudioBuffer)));
        player->coreAudioBuffers[i]->mNumberBuffers = 1;
        player->coreAudioBuffers[i]->mBuffers[0].mNumberChannels = 2;
        player->coreAudioBuffers[i]->mBuffers[0].mDataByteSize = dataByteSize;
        player->coreAudioBuffers[i]->mBuffers[0].mData = malloc(dataByteSize);
    }

    player->totalFrames = (u64)totalFrames;
    player->currentFrame = 0;
    player->currentBufferIndex = 0;
    player->flags = flags;
    player->threadPool = &ctx->threadPool;
    player->id = numWavPlayers_;
    numWavPlayers_++;

    // Preload both buffers
    LoadNextChunk((void*)player);
    atomic_store(&player->currentBufferIndex, 1);
    LoadNextChunk((void*)player);
    atomic_store(&player->currentBufferIndex, 0);

    CFRelease(url);

    return CoreEngine_CreateProcessor(ctx, ProcessWavPlayer, DestroyWavPlayer, (void*)player);
}

