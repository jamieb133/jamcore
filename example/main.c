#include "utils.h"
#include <core_engine.h>
#include <oscillators.h>
#include <passthrough.h>
#include <time.h>
#include <logger.h>
#include <fader.h>
#include <wav_player.h>
#include <iir_filter.h>
#include <audio_renderer.h>

int main()
{
    SetLogLevel(LOG_INFO);

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

    // Create a lowpass IIR filter
    IirFilter lowpass;
    u16 filterId = IirFilter_Create(&lowpass, &context, SAMPLE_RATE_DEFAULT, IIR_LOWPASS, 100, 1, 1);

    // Create a renderer to record the audio to a file
    AudioRenderer renderer;
    u16 rendererId = AudioRenderer_Create(&renderer, &context, "Example_Rendered.wav");

    // Create a channel fader for each audio source
    Fader sqFader, sinFader, wavFader, recordFader;
    u16 channelSquareWave, channelSineWave, channelWavPlayer, channelRenderer;
    channelSquareWave = Fader_Create(&sqFader, -1.0f, 0.0f, &context);
    channelSineWave = Fader_Create(&sinFader, 1.0f, 0.0f, &context);
    channelWavPlayer = Fader_Create(&wavFader, 0.0f, 0.0f, &context);
    channelRenderer = Fader_Create(&recordFader, 0.0f, 1.0f, &context);

    // Route the sin signal to mixer control
    CoreEngine_Route(&context, sinOscId, channelSineWave, true);
    // Route the saw signal to mixer control
    CoreEngine_Route(&context, sqOscId, channelSquareWave, true);
    // Apply lowpass filter to wav audio
    CoreEngine_Route(&context, wavPlayerId, filterId, true);
    // Route the filtered wav file audio to mixer control
    CoreEngine_Route(&context, filterId, channelWavPlayer, true);

    // Record input file without filtering
    CoreEngine_Route(&context, sinOscId, channelRenderer, true);
    CoreEngine_Route(&context, sqOscId, channelRenderer, true);
    CoreEngine_Route(&context, wavPlayerId, channelRenderer, true);

    // Connect Fader 4 output to Renderer
    CoreEngine_Route(&context, channelRenderer, rendererId, true);

    // Start recording
    AudioRenderer_StartRecord(&renderer);

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
            if (lowpass.type == IIR_LOWPASS) {
                lowpass.type = IIR_HIGHPASS;
                lowpass.freq = 1000;
            }
            else {
                lowpass.type = IIR_LOWPASS;
                lowpass.freq = 100;
            }
            IirFilter_Recalculate(&lowpass);
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

    // Stop recording
    AudioRenderer_StopRecord(&renderer);

    // Stop audio
    CoreEngine_Stop(&context);

    // Deinit
    CoreEngine_Deinit(&context);

    return 0;
}
