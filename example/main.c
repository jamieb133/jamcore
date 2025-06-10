#include <core_engine.h>
#include <oscillators.h>
#include <passthrough.h>
#include <time.h>
#include <logger.h>

int main()
{
    CoreEngineContext context;
    memset(&context, 0, sizeof(CoreEngineContext));

    // Initialise Core Engine
    CoreEngine_Init(&context, 0.25, 1024);

    // Create a passthrough component that allows us to split the signal at the root
    // effectively enabling multiple input sources.
    u16 passthroughId = Passthrough_Create(&context);
    CoreEngine_SetSource(&context, passthroughId);

    // Create sin osc
    Oscillator sinOsc;
    u16 sinProcId = Oscillator_Create(&sinOsc, &context, WAVEFORM_SIN, 440.0, 0.0, 0.5);

    // Create sawtooth osc
    Oscillator sawOsc;
    u16 sawProcId = Oscillator_Create(&sawOsc, &context, WAVEFORM_SAW, 440.0, 0.0, 0.01);

    // Route the source stream to each oscillator in parallel
    CoreEngine_Route(&context, passthroughId, sinProcId, true);
    CoreEngine_Route(&context, passthroughId, sawProcId, true);

    // Start audio
    CoreEngine_Start(&context);

    usleep(2000000);

    // Stop audio
    CoreEngine_Stop(&context);

    return 0;
}
