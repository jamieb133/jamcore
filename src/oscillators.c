#include <MacTypes.h>
#include <math.h>
#include <stdlib.h>

#include <core_engine.h>
#include <logger.h>
#include <allocator.h>
#include <oscillators.h>

static const char* waveformStrings_[WAVEFORM_COUNT] = {
    "WAVEFORM_SIN",
    "WAVEFORM_SQUARE",
    "WAVEFORM_SAW",
};

static void ProcessCallback(JamAudioProcessor* self, int numFrames, double sampleRate, float**buffers, UInt16 numBuffers)
{
    LogInfoOnce("frames: %d sampleRate: %f", numFrames, sampleRate);

    Oscillator* osc = (Oscillator*)self->procData;
    Assert(osc, "Oscillator is null");

    double phaseIncrement = (2.0 * M_PI * osc->frequency) / sampleRate;

    for (UInt16 i = 0; i < numFrames; i++) {
        float sample = 0;
        switch (osc->type) {
            case WAVEFORM_SIN:
                sample = (float)sin(osc->phase);
                break;
            case WAVEFORM_SQUARE:
                sample = (osc->phase < M_PI) ? 1.0 : -1.0;
                break;
            case WAVEFORM_SAW:
                sample = (float)((osc->phase / (2.0 * M_PI)) * 2.0 - 1.0);
                break;
            default:
                Assert(false, "Unknown waveform type %d", osc->type);
        }

        UInt16 baseSampleIndex = i * 2;

        for (UInt16 j = 0; j < numBuffers; j++) {
            buffers[j][baseSampleIndex] = sample; // Left channel
            buffers[j][baseSampleIndex + 1] = sample; // Right channel
        }

        osc->phase += phaseIncrement;
        while (osc->phase >= 2.0 * M_PI) {
            osc->phase -= 2.0 * M_PI;
        }
    }
}

JamAudioProcessor* Oscillator_Create(Oscillator* osc, 
                                    WaveformId type,
                                    double frequency,
                                    double phase,
                                    CoreEngineContext* engine)
{
    Assert(osc != NULL, "Oscillator is NULL");
    Assert(type < WAVEFORM_COUNT, "Waveform type ID invalid");

    LogInfo("Creating %s Oscillator", waveformStrings_[type]);

    ZeroObj(Oscillator, osc);
    osc->phase = phase;
    osc->frequency = frequency;
    osc->type = type;

    return CoreEngine_CreateProcessor(engine, ProcessCallback, NULL, (void*)osc);
}

