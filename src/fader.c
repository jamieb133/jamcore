#include "core_engine.h"
#include "logger.h"
#include <math.h>
#include <fader.h>
#include <utils.h>

static void ProcessCallback(f64 sampleRate, u16 numFrames, f32* buffer, void* data)
{
    (void)sampleRate;

    Fader* fader = (Fader*)data;
    Assert(fader, "Fader is null");

    f32 vol = Clamp(fader->vol, 0.0f, 1.0f);
    f32 pan = Clamp(fader->pan, -1.0f, 1.0f);
    f32 angle = (pan + 1) * (M_PI / 4.0f);
    f32 leftGain = cosf(angle) * vol;
    f32 rightGain = sinf(angle) * vol;

    for (u16 i = 0; i < numFrames; i++) {
        u16 sampleIndex = i * 2;
        buffer[sampleIndex] *= leftGain; 
        buffer[sampleIndex + 1] *= rightGain; 
    }
}

u16 Fader_Create(Fader* fader, f32 defaultPan, f32 defaultVol, CoreEngineContext* ctx)
{
    LogInfo("Creating Fader: pan = %f, vol = %f", defaultPan, defaultVol);
    Assert(fader, "Fader is null");
    fader->pan = defaultPan;
    fader->vol = defaultVol;
    return CoreEngine_CreateProcessor(ctx, ProcessCallback, NULL, (void*)fader) ;
}

