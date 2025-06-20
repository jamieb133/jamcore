#include "logger.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudioTypes/CoreAudioBaseTypes.h>
#include <CoreServices/CoreServices.h>
#include <MacTypes.h>
#include <string.h>
#include <wav_player.h>

static void ProcessWavPlayer(f64 sampleRate, u16 numFrames, f32* buffer, void* data)
{
    (void)sampleRate;

    WavPlayer* player = (WavPlayer*)data;
    Assert(player, "WavPlayer is null");

    if (player->currentFrame >= player->totalFrames) {
        return;
    }

    u64 remainingFrames = player->totalFrames - player->currentFrame;
    u16 framesThisTime = (remainingFrames < numFrames) ? remainingFrames : numFrames;
    f32* wavBuffer = (f32*)player->coreAudioBuffers->mBuffers[0].mData;
    u32 baseSampleIndex = player->currentFrame * 2;

    for (u16 i = 0; i < framesThisTime; i++) {
        u32 sampleIndex = baseSampleIndex + i * 2;
        buffer[i * 2] += wavBuffer[sampleIndex];
        buffer[i * 2 + 1] += wavBuffer[sampleIndex + 1];
    }

    player->currentFrame += framesThisTime;

    if ((player->currentFrame >= player->totalFrames) && (player->looping)) {
        player->currentFrame = 0; 
    }
}

static void DestroyWavPlayer(void* data)
{
    WavPlayer* player = (WavPlayer*)data;
    Assert(player, "WavPlayer is null");
    LogInfo("Destroying WavPlayer");
    free(player->coreAudioBuffers->mBuffers[0].mData);
    free(player->coreAudioBuffers);
}

u16 WavPlayer_Create(WavPlayer* player, CoreEngineContext* ctx, const char* filename, bool looping)
{
    Assert(player != NULL, "WavPlayer is NULL");
    Assert(ctx != NULL, "CoreEngineContext is NULL");

    LogInfo("Creating WavPlayer for %s", filename);

    OSStatus status;

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        NULL/*TODO: use custom heap*/,
        (const u8*)filename,
        strlen(filename),
        false // IsDirectory
    );

    ExtAudioFileRef audioFile;
    status = ExtAudioFileOpenURL(url, &audioFile);
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

    status = ExtAudioFileSetProperty(audioFile, kExtAudioFileProperty_ClientDataFormat, sizeof(streamFormat), &streamFormat);
    Assert(status == noErr, "Failed to set file properties for %s", filename);

    i64 totalFrames;
    u32 propSize = sizeof(totalFrames);
    status = ExtAudioFileGetProperty(audioFile, kExtAudioFileProperty_FileLengthFrames, &propSize, &totalFrames);
    Assert(status == noErr, "Failed to get file properties for %s", filename);
    Assert(totalFrames > 0, "Total frames read in %s was 0", filename);

    u32 dataByteSize = totalFrames * streamFormat.mBytesPerFrame;

    player->coreAudioBuffers = malloc(sizeof(AudioBufferList) + sizeof(AudioBuffer));
    player->coreAudioBuffers->mNumberBuffers = 1;
    player->coreAudioBuffers->mBuffers[0].mNumberChannels = 2;
    player->coreAudioBuffers->mBuffers[0].mDataByteSize = dataByteSize;
    player->coreAudioBuffers->mBuffers[0].mData = malloc(dataByteSize);

    u32 framesToRead = (u32)totalFrames;
    status = ExtAudioFileRead(audioFile, &framesToRead, player->coreAudioBuffers);
    Assert(status == noErr, "Failed to load %s", filename);

    player->totalFrames = (u64)framesToRead;
    player->currentFrame = 0;
    player->looping = looping;

    CFRelease(url);
    ExtAudioFileDispose(audioFile);

    return CoreEngine_CreateProcessor(ctx, ProcessWavPlayer, DestroyWavPlayer, (void*)player);
}

