#include <MacTypes.h>
#include <math.h>
#include <stdlib.h>

#include <core_engine.h>
#include <logger.h>
#include <allocator.h>
#include <oscillators.h>

static void OscillatorProcessor_Process(JamAudioProcessor* self)
{
    LogInfoOnce("frames: %d sampleRate: %f", self->numFrames, self->sampleRate);

    Oscillator* osc = (Oscillator*)self->procData;
    Assert(osc, "Oscillator is null");

    double phaseIncrement = (2.0 * M_PI * osc->frequency) / self->sampleRate;

    for (UInt16 i = 0; i < self->numFrames; i++) {
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

        self->buffer[baseSampleIndex] = sample; // Left channel
        self->buffer[baseSampleIndex + 1] = sample; // Right channel

        osc->phase += phaseIncrement;
        while (osc->phase >= 2.0 * M_PI) {
            osc->phase -= 2.0 * M_PI;
        }
    }
}

static void OscillatorProcessor_Destroy(JamAudioProcessor* self)
{
    LogInfo("Destroying Oscillator");
    Dealloc(self);
}

void Oscillator_Create(Oscillator* osc, 
                        WaveformId type,
                        double frequency,
                        double phase,
                        CoreEngineContext* engine, 
                        JamAudioChannelId channelId)
{
    Assert(osc != NULL, "Oscillator is NULL");

    ZeroObj(Oscillator, osc);
    osc->phase = phase;
    osc->frequency = frequency;
    osc->type = type;

    JamAudioProcessor* processor = AllocOne(JamAudioProcessor);
    processor->procData = (void*)osc;
    processor->Process = OscillatorProcessor_Process;
    processor->Destroy = OscillatorProcessor_Destroy;

    CoreEngine_AddProcessor(engine, channelId, processor);
}

