#include <core_engine.h>
#include <oscillators.h>
#include <time.h>

int main()
{
    CoreEngineContext context;
    memset(&context, 0, sizeof(CoreEngineContext));

    // Initialise Core Engine
    CoreEngine_Init(&context, 0.5);

    // Create a channel to route the sin wave
    JamAudioChannelId channelId = CoreEngine_CreateChannel(&context);

    Oscillator sinOsc;
    Oscillator_Create(&sinOsc, WAVEFORM_SIN, 440.0, 0.0, &context, channelId);

    // Start audio
    CoreEngine_Start(&context);

    // Run the engine for 5 seconds
    usleep(5000000);

    // Stop audio
    CoreEngine_Stop(&context);

    return 0;
}
