#include "logger.h"
#include <passthrough.h>

static void ProcessPassthrough(f64 sampleRate, u16 numFrames, f32* buffer, void* data)
{
    // Do absolutely nothing
    (void)sampleRate;
    (void)numFrames;
    (void)buffer;
    (void)data;
}

u16 Passthrough_Create(CoreEngineContext* ctx)
{
    LogInfo("Creating Passthrough Processor");
    return CoreEngine_CreateProcessor(ctx, ProcessPassthrough, NULL, NULL, NULL);
}
