#include "utils.h"
#include <core_engine.h>
#include <oscillators.h>
#include <passthrough.h>
#include <time.h>
#include <logger.h>
#include <fader.h>

int main()
{
    CoreEngineContext context;
    memset(&context, 0, sizeof(CoreEngineContext));

    // Initialise Core Engine
    CoreEngine_Init(&context, 0.5, 1024);

    // Create a passthrough component that allows us to split the signal at the root
    // effectively enabling multiple input sources.
    u16 passthroughId = Passthrough_Create(&context);
    CoreEngine_SetSource(&context, passthroughId);

    // Create sin osc
    Oscillator sinOsc;
    u16 sinOscId = Oscillator_Create(&sinOsc, &context, WAVEFORM_SIN, 440.0, 0.0, 0.5);

    // Create squarewave osc
    Oscillator sqOsc;
    u16 sqOscId = Oscillator_Create(&sqOsc, &context, WAVEFORM_SQUARE, 440.0, 0.0, 0.1);

    // Create a channel fader for each oscillator
    Fader sqFader, sinFader;
    u16 faderId1, faderId2;
    faderId1 = Fader_Create(&sqFader, -1.0f, 0.0f, &context);
    faderId2 = Fader_Create(&sinFader, 1.0f, 0.0f, &context);

    // Route the sin signal to mixer control
    CoreEngine_Route(&context, passthroughId, sinOscId, true);
    CoreEngine_Route(&context, sinOscId, faderId2, true);

    // Route the saw signal to mixer control
    CoreEngine_Route(&context, passthroughId, sqOscId, true);
    CoreEngine_Route(&context, sqOscId, faderId1, true);

    // Start audio
    CoreEngine_Start(&context);

    i32 windowMs = 2000;
    i32 remaining;

    {
        // Fade in
        remaining = windowMs;
        f32 scale = (0.5f / (f32)windowMs);
        while (remaining--) {
            sinFader.vol += scale; 
            sqFader.vol += scale; 
            usleep(1000);
        }
    }

    {
        // Sweep across
        remaining = windowMs;
        f32 scale = (2.0f / (f32)windowMs);
        while (remaining--) {
            sinFader.pan -= scale; 
            sqFader.pan += scale; 
            usleep(1000);
        }
    }

    { // Sweep back
        remaining = windowMs;
        f32 scale = (2.0f / (f32)windowMs);
        while (remaining--) {
            sinFader.pan += scale; 
            sqFader.pan -= scale; 
            usleep(1000);
        }
    }

    { // Fade out sin
        remaining = windowMs;
        f32 scale = (1.0f / (f32)windowMs);
        while (remaining--) {
            sinFader.vol -= scale;
            sinFader.vol = ClampLow(sinFader.vol, 0.0f);
            usleep(1000);
        }
    }

    { // Fade out square
        remaining = windowMs;
        f32 scale = (1.0f / (f32)windowMs);
        while (remaining--) {
            sqFader.vol -= scale; 
            sqFader.vol = ClampLow(sqFader.vol, 0.0f);
            usleep(1000);
        }
    }

    // Stop audio
    CoreEngine_Stop(&context);

    return 0;
}
