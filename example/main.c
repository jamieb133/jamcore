#include "utils.h"
#include <core_engine.h>
#include <oscillators.h>
#include <passthrough.h>
#include <time.h>
#include <logger.h>
#include <fader.h>
#include <wav_player.h>

int main()
{
    CoreEngineContext context;
    memset(&context, 0, sizeof(CoreEngineContext));

    // Initialise Core Engine
    CoreEngine_Init(&context, 0.5, 1024);

    // Create sin osc
    Oscillator sinOsc;
    u16 sinOscId = Oscillator_Create(&sinOsc, &context, WAVEFORM_SIN, 440.0, 0.0, 0.5);
    CoreEngine_AddSource(&context, sinOscId);

    // Create squarewave osc
    Oscillator sqOsc;
    u16 sqOscId = Oscillator_Create(&sqOsc, &context, WAVEFORM_SQUARE, 440.0, 0.0, 0.015);
    CoreEngine_AddSource(&context, sqOscId);

    // Create WAV Player
    WavPlayer wavPlayer;
    u16 wavPlayerId = WavPlayer_Create(&wavPlayer, &context, "example/Kings.wav", WAVPLAYER_LOOPING);
    CoreEngine_AddSource(&context, wavPlayerId);

    // Create a channel fader for each oscillator
    Fader sqFader, sinFader, wavFader;
    u16 faderId1, faderId2, faderId3;
    faderId1 = Fader_Create(&sqFader, -1.0f, 0.0f, &context);
    faderId2 = Fader_Create(&sinFader, 1.0f, 0.0f, &context);
    faderId3 = Fader_Create(&wavFader, 0.0f, 0.0f, &context);

    // Route the sin signal to mixer control
    CoreEngine_Route(&context, sinOscId, faderId2, true);
    // Route the saw signal to mixer control
    CoreEngine_Route(&context, sqOscId, faderId1, true);
    // Route the wav file audio to mixer control
    CoreEngine_Route(&context, wavPlayerId, faderId3, true);

    // Start audio
    CoreEngine_Start(&context);

    i32 windowMs = 2000;
    i32 remaining;

    {
        // Fade in
        remaining = windowMs;
        f32 scale = (0.3f / (f32)windowMs);
        while (remaining--) {
            sinFader.vol += scale; 
            sqFader.vol += scale; 
            wavFader.vol += scale;
            usleep(1000);
        }
    }

    { // Seek wav file
        i32 twoSeconds = 5;
        while (twoSeconds--) {
            u32 seekPosition = 300000 * twoSeconds;
            LogInfo("Seeking wav file to %d", seekPosition);
            WavPlayer_Seek(&wavPlayer, seekPosition);
            usleep(2000000);
        }
    }

    {
        // Sweep across
        remaining = windowMs;
        f32 scale = (2.0f / (f32)windowMs);
        f32 pitchScale = (440.0f / (f32) windowMs);
        while (remaining--) {
            sinFader.pan -= scale; 
            sqFader.pan += scale; 
            sinOsc.frequency += pitchScale;
            sqOsc.frequency -= pitchScale;
            usleep(1000);
        }
    }

    { // Sweep back
        remaining = windowMs;
        f32 scale = (2.0f / (f32)windowMs);
        f32 pitchScale = (440.0f / (f32) windowMs);
        while (remaining--) {
            sinFader.pan += scale; 
            sqFader.pan -= scale; 
            sinOsc.frequency -= pitchScale;
            sqOsc.frequency += pitchScale;
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

    { // Fade out wav audio
        remaining = windowMs;
        f32 scale = (1.0f / (f32)windowMs);
        while (remaining--) {
            wavFader.vol -= scale; 
            wavFader.vol = ClampLow(wavFader.vol, 0.0f);
            usleep(1000);
        }
    }

    // Stop audio
    CoreEngine_Stop(&context);

    // Deinit
    CoreEngine_Deinit(&context);

    return 0;
}
