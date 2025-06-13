#include "fake_processor.h"
#include "logger.h"
#include <pthread.h>
#include <sys/errno.h>
#include <time.h>

static void ProcessFake(f64 sampleRate, u16 numFrames, f32* buffer, void* data)
{
    FakeProcessor* proc = (FakeProcessor*)data;
    Assert(proc, "Expected valid FakeProcessor but got null"); 
    Assert(proc->buffer, "Fake processor buffer null");

    LogInfoPeriodic(1000, "Fake processor callback invoked");

    pthread_mutex_lock(&proc->mutex);

    proc->numFrames = numFrames;
    proc->sampleRate = sampleRate;
    proc->receivedData = true;

    if ((numFrames * 2) > proc->capacity) {
        pthread_mutex_unlock(&proc->mutex);
        Assert(false, "Number of received frames exceeds capacity of fake processor buffer");
    }

    memcpy(proc->buffer, buffer, numFrames * 2);

    pthread_mutex_unlock(&proc->mutex);
    pthread_cond_signal(&proc->cond);
}

static void DestroyFake(void* data)
{
    FakeProcessor* proc = (FakeProcessor*)data;
    Assert(proc, "Expected valid FakeProcessor but got null"); 

    proc->receivedData = false;
    proc->capacity = 0;
    proc->numFrames = 0;

    free(proc->buffer);
    pthread_mutex_destroy(&proc->mutex);
    pthread_cond_destroy(&proc->cond);
}

u16 FakeProcessor_Create(FakeProcessor* proc, CoreEngineContext* ctx, u16 bufferSize)
{
    pthread_mutex_init(&proc->mutex, NULL);
    pthread_cond_init(&proc->cond, NULL);

    proc->capacity = bufferSize;
    proc->buffer = malloc(bufferSize);
    proc->numFrames = 0;
    proc->receivedData = false;

    return CoreEngine_CreateProcessor(ctx, ProcessFake, DestroyFake, (void*)proc);
}

bool FakeProcessor_WaitForData(FakeProcessor *proc)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 500 * 1000 * 1000; // Wait half sec
    ts.tv_sec += ts.tv_nsec / NSEC_PER_SEC;
    ts.tv_nsec %= NSEC_PER_SEC;

    while (!proc->receivedData) {
        i32 res = pthread_cond_timedwait(&proc->cond, &proc->mutex, &ts);
        if (res == ETIMEDOUT) {
            return false;
        }
    }

    return true;
}

