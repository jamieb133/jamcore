#pragma once

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "types.h"

typedef void (*TaskCallback)(void*);

typedef struct {
    TaskCallback callback;
    void* data;
} TaskInfo;

typedef struct {
    TaskInfo* tasks;
    u64 capacity;
    _Atomic(u64) numPendingTasks;
    _Atomic(bool) running;
    u8 numThreads;
    pthread_t* threads;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ThreadPool;

void ThreadPool_Init(ThreadPool* pool, u8 numThreads, u64 capacity);
void ThreadPool_Deinit(ThreadPool* pool);
void ThreadPool_Start(ThreadPool* pool);
void ThreadPool_Stop(ThreadPool* pool);
void ThreadPool_DeferTask(ThreadPool* pool, TaskCallback callback, void* data);
void ThreadPool_FlushTasks(ThreadPool* pool);

