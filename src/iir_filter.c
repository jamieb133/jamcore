#include "core_engine.h"
#include "thread_pool.h"
#include <iir_filter.h>
#include <logger.h>

static void CalculateCoeffs(void* data)
{
    IirFilter* filter = (IirFilter*)data;
    Assert(filter, "Filter is null");

    f32 A, omega, alpha;

    A = pow(10, filter->dbGain/(40.0 * filter->freq));
    omega = (2 * M_PI * filter->freq) / filter->sampleRate;
    alpha = sin(omega) / (2.0 * filter->qFactor);

    switch (filter->type) {
        case IIR_LOWPASS:
            filter->coeffs.a0 = (1 + alpha) ;
            filter->coeffs.a1 = -2 * cos(omega);
            filter->coeffs.a2 = 1 - alpha;
            filter->coeffs.b0 = (1 - cos(omega)) / 2;
            filter->coeffs.b1 = 1 - cos(omega);
            filter->coeffs.b2 = filter->coeffs.b0;
        break;

        case IIR_HIGHPASS:
            filter->coeffs.a0 = (1 + alpha);
            filter->coeffs.a1 = -2 * cos(omega);
            filter->coeffs.a2 = 1 - alpha;
            filter->coeffs.b0 = (1 + cos(omega)) / 2;
            filter->coeffs.b1 = -(1 + cos(omega));
            filter->coeffs.b2 = filter->coeffs.b0;
        break;

        case IIR_BANDPASS:
            filter->coeffs.b0 = 1 + (alpha * A);
            filter->coeffs.b1 = -2 * cos(omega);
            filter->coeffs.b2 = 1 - (alpha * A);
            filter->coeffs.a0 = 1 + (alpha / A);
            filter->coeffs.a1 = -2 * cos(omega);
            filter->coeffs.a2 = 1 - (alpha / A);
        break;

        case IIR_BANDSTOP:
            filter->coeffs.a0 = (1 + alpha);
            filter->coeffs.a1 = -2 * cos(omega);
            filter->coeffs.a2 = 1 - alpha;
            filter->coeffs.b0 = filter->qFactor * alpha;
            filter->coeffs.b1 = 0;
            filter->coeffs.b2 = -filter->qFactor * alpha;

        case IIR_HIGH_SHELVE: // TODO
        case IIR_LOW_SHELVE: // TODO
        default:
            Assert(false, "Unknown IIR filter type");
        break;
    }

    LogInfo("Calculated IIR coefficients: a0=%f, a1=%f, a2=%f, b0=%f, b1=%f, b2=f%",
                filter->coeffs.a0,
                filter->coeffs.a1,
                filter->coeffs.a2,
                filter->coeffs.b0,
                filter->coeffs.b1,
                filter->coeffs.b2);
}

static f32 FilterSample(IirFilter* filter, f32 sample, u8 index)
{
    Assert(index < 2, "Sample channel index can only be 0 (left) or 1 (right)");

    f32 current_output_y_n; // This will hold y[n]

    // Calculate the current output y[n]
    // Combination of feedforward and feedback terms
    current_output_y_n = (filter->coeffs.b0 * sample) // b0*x[n]
                       + (filter->coeffs.b1 * filter->buffers[index].inputs[0]) // b1*x[n-1]
                       + (filter->coeffs.b2 * filter->buffers[index].inputs[1]) // b2*x[n-2]
                       - (filter->coeffs.a1 * filter->buffers[index].outputs[0]) // a1*y[n-1]  (Note the minus sign for feedback)
                       - (filter->coeffs.a2 * filter->buffers[index].outputs[1]); // a2*y[n-2] (Note the minus sign for feedback)

    Assert(filter->coeffs.a0 != 0, "IIR a0 cannot be zero for valid filter");
    current_output_y_n /= filter->coeffs.a0;

    // Shift buffer values for the NEXT sample
    filter->buffers[index].inputs[1] = filter->buffers[index].inputs[0]; // x[n-2] = x[n-1]
    filter->buffers[index].inputs[0] = sample;                           // x[n-1] = x[n] (current input for next iteration)
    filter->buffers[index].outputs[1] = filter->buffers[index].outputs[0]; // y[n-2] = y[n-1]
    filter->buffers[index].outputs[0] = current_output_y_n;              // y[n-1] = y[n] (current output for next iteration)

    LogInfoPeriodic(1000, "%f -> %f", sample, current_output_y_n);

    return current_output_y_n;
}

static void ProcessIirFilter(f64 sampleRate, u16 numFrames, f32* buffer, void* data)
{
    IirFilter* filter = (IirFilter*)data;
    Assert(filter, "Filter is null");

    for (u16 i = 0; i < numFrames; i++) {
        u16 baseSampleIndex = i * 2;
        buffer[baseSampleIndex] = FilterSample(filter, buffer[baseSampleIndex], 0);
        buffer[baseSampleIndex + 1] = FilterSample(filter, buffer[baseSampleIndex + 1], 1);
    }

    if (filter->flags & IIR_RECALCULATE) {
        filter->sampleRate = sampleRate;
        ThreadPool_DeferTask(filter->threadPool, CalculateCoeffs, (void*)filter);
    }
}

u16 IirFilter_Create(IirFilter *filter, 
                     CoreEngineContext *ctx, 
                     f64 sampleRate,
                     FilterType type, 
                     f32 freq,
                     f32 qFactor, 
                     f32 atten)
{
    Assert(filter, "Filter is null");

    filter->sampleRate = sampleRate;
    filter->type = type;
    filter->freq = freq;
    filter->qFactor = qFactor;
    filter->atten = atten;
    filter->flags = 0;
    filter->threadPool = &ctx->threadPool;

    CalculateCoeffs((void*)filter);

    return CoreEngine_CreateProcessor(ctx, ProcessIirFilter, NULL, (void*)filter); 
}

void IirFilter_Recalculate(IirFilter *filter)
{
    Assert(filter, "Filter is null");
    filter->flags |= IIR_RECALCULATE;
}

