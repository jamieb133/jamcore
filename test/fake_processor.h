#pragma once

#include <stdbool.h>
#include <types.h>
#include <pthread.h>
#include <stdatomic.h>

#include <core_engine.h>
#include <logger.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    _Atomic(bool) receivedData;
    _Atomic(bool) newAudioCycleCalled;
    u16 capacity;
    f64 sampleRate;
    u16 numFrames;
    f32* buffer;
} FakeProcessor;

void FakeProcessor_InitValues(FakeProcessor* proc, u16 bufferSize);
u16 FakeProcessor_Create(FakeProcessor* proc, CoreEngineContext* ctx, u16 bufferSize);
bool FakeProcessor_WaitForData(FakeProcessor* proc);

