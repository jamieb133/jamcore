#include "test_framework.h"
#include <string.h>
#include <thread_pool.h>
#include <unistd.h>

static void TestCallback(void* data)
{
    _Atomic(u8)* count = (_Atomic(u8)*)data;
    Assert(count, "TaskCallback data is NULL");
    atomic_fetch_add(count, 1);
    LogInfo("Thread = %d, Task = %d", pthread_self(), atomic_load(count));
}

TEST(ThreadPool, RunTasks)
{
    u8 numThreads = 4;
    u8 numTasks = 10;

    ThreadPool pool;
    ThreadPool_Init(&pool, numThreads, 10);
    ThreadPool_Start(&pool);

    _Atomic(u8) count = 0;

    for (u8 i = 0; i < numTasks; i++) 
        ThreadPool_DeferTask(&pool, TestCallback, (void*)&count);

    ThreadPool_FlushTasks(&pool);
    ThreadPool_Stop(&pool);
    ThreadPool_Deinit(&pool);

    CHECK_TRUE(count == numTasks);
}

TEST_SETUP(ThreadPool)
{
    ADD_TEST(ThreadPool, RunTasks);
}

TEST_BRINGUP(ThreadPool)
{

}

TEST_TEARDOWN(ThreadPool)
{

}

