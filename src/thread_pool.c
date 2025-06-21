#include <stdlib.h>
#include <logger.h>
#include <thread_pool.h>

static void* Worker(void* data)
{
    ThreadPool* pool = (ThreadPool*) data;
    Assert(pool, "ThreadPool is null");

    while (atomic_load(&pool->running) || (atomic_load(&pool->numPendingTasks) > 0)) {
        pthread_mutex_lock(&pool->mutex);

        while (pool->numPendingTasks == 0 && pool->running) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }

        if (pool->numPendingTasks == 0 && !pool->running) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        TaskInfo task = pool->tasks[pool->numPendingTasks - 1];
        atomic_fetch_sub(&pool->numPendingTasks, 1);
        pthread_mutex_unlock(&pool->mutex);

        task.callback(task.data);
    }

    return NULL;
}

void ThreadPool_Init(ThreadPool* pool, u8 numThreads, u64 capacity)
{
    LogInfo("Creating Thread Pool with %d threads and capacity of %d tasks", numThreads, capacity);

    Assert(pool, "ThreadPool is null");
    Assert(numThreads > 0, "numThreads must be > 0");
    Assert(capacity > 0, "Job queue capacity must be > 0");
    
    pool->numPendingTasks = 0;
    pool->numThreads = numThreads;
    pool->tasks = malloc(capacity * sizeof(TaskInfo));
    pool->threads = malloc(numThreads * sizeof(pthread_t));
    pool->running = true;
    pool->capacity = capacity;

    Assert(pthread_mutex_init(&pool->mutex, NULL) == 0, "Failed to create mutex");
    Assert(pthread_cond_init(&pool->cond, NULL) == 0, "Failed to create condition variable");
}

void ThreadPool_Deinit(ThreadPool* pool)
{
    LogInfo("Deinitializing Thread Pool");
    Assert(pool, "ThreadPool is null");

    pool->numPendingTasks = 0;
    pool->numThreads = 0;
    free(pool->tasks);
    free(pool->threads);

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
}

void ThreadPool_Start(ThreadPool* pool)
{
    LogInfo("Starting Thread Pool");
    Assert(pool, "ThreadPool is null");

    for (u8 i = 0; i < pool->numThreads; i++)
        Assert(pthread_create(&pool->threads[i], NULL, Worker, (void*)pool) == 0, "Failed to create thread");
}

void ThreadPool_Stop(ThreadPool* pool)
{
    LogInfo("Stopping Thread Pool");
    Assert(pool, "ThreadPool is null");

    atomic_store(&pool->running, false);
    pthread_cond_broadcast(&pool->cond);

    for (u8 i = 0; i < pool->numThreads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
}

void ThreadPool_DeferTask(ThreadPool* pool, TaskCallback callback, void* data)
{
    Assert(pool, "ThreadPool is null");
    Assert(pool->numPendingTasks < pool->capacity, "Number of tasks reached capacity");
    
    // NOTE: not thread safe, intended only to be called by the audio thread

    pool->tasks[pool->numPendingTasks] = (TaskInfo) {
        .callback = callback,
        .data = data,
    };
    atomic_fetch_add(&pool->numPendingTasks, 1);
}

void ThreadPool_FlushTasks(ThreadPool* pool)
{
    if (atomic_load(&pool->numPendingTasks) > 0) {
        pthread_cond_broadcast(&pool->cond);
    }
}

