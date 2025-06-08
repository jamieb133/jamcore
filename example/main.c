#include <core_engine.h>
#include <oscillators.h>
#include <time.h>
#include <logger.h>

int main()
{
    CoreEngineContext context;
    memset(&context, 0, sizeof(CoreEngineContext));

    // Initialise Core Engine
    CoreEngine_Init(&context, 0.25);


    // Create sin osc
    Oscillator sinOsc;
    JamAudioProcessor* sinProc = Oscillator_Create(&sinOsc, WAVEFORM_SIN, 440.0, 0.0, &context);

    // Create sawtooth osc
    Oscillator sawOsc;
    JamAudioProcessor* sawProc = Oscillator_Create(&sawOsc, WAVEFORM_SAW, 440.0, 0.0, &context);

    // Route the oscillators through channels one and two
    Processor_RouteChannel(sinProc, 0, true);
    Processor_RouteChannel(sawProc, 1, true);

    // Adjust channel controls
    JamAudioChannel* channel0 = &context.channels[0];
    JamAudioChannel* channel1 = &context.channels[1];
    channel0->volume = 255;
    channel1->volume = 25;
    channel0->pan = 0; 
    channel1->pan = 255; 

    // Start audio
    CoreEngine_Start(&context);

    // Unmute sin wave
    CoreEngine_EnableChannel(&context, 0, true);

    usleep(2000000);

    // Solo the sawtooth wave
    CoreEngine_EnableChannel(&context, 0, false);
    CoreEngine_EnableChannel(&context, 1, true);

    usleep(2000000);

    // Play both
    CoreEngine_EnableChannel(&context, 0, true);
    CoreEngine_EnableChannel(&context, 1, true);

    usleep(2000000);

    // Stop audio
    CoreEngine_Stop(&context);

    return 0;
}
