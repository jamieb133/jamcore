#include "test_framework.h"
#include "fake_processor.h"
#include <core_engine.h>
#include <os/workgroup.h>

static inline bool IsFlagSet(CoreEngineContext* ctx, u8 flag)
{
    return ctx->flags & (1 << flag);
}

TEST(CoreEngine, Init)
{
    CoreEngineContext ctx;

    CHECK_DEATH(CoreEngine_Init(NULL, 0, 0)); // NULL context
    CHECK_DEATH(CoreEngine_Init(&ctx, 0, 0)); // Invalid heap size

    CoreEngine_Init(&ctx, 0, 4096);

    CHECK_TRUE(IsFlagSet(&ctx, ENGINE_INITIALIZED));
    CHECK_TRUE(!IsFlagSet(&ctx, ENGINE_STARTED));
    CHECK_TRUE(!IsFlagSet(&ctx, ENGINE_STOP_REQUESTED));
    CHECK_TRUE(!IsFlagSet(&ctx, ENGINE_AUDIO_THREAD_SILENCED));

    CoreEngine_Deinit(&ctx);
}

TEST(CoreEngine, Deinit)
{
    CoreEngineContext ctx;

    CHECK_DEATH(CoreEngine_Deinit(&ctx)); // Not initialised yet     

    CoreEngine_Init(&ctx, 0, 4096);
    CoreEngine_Deinit(&ctx);

    CHECK_TRUE(!IsFlagSet(&ctx, ENGINE_INITIALIZED));
    CHECK_TRUE(!IsFlagSet(&ctx, ENGINE_STARTED));
    CHECK_TRUE(!IsFlagSet(&ctx, ENGINE_STOP_REQUESTED));
    CHECK_TRUE(!IsFlagSet(&ctx, ENGINE_AUDIO_THREAD_SILENCED));
}

TEST(CoreEngine, Start)
{
    CoreEngineContext ctx;
    
    CHECK_DEATH(CoreEngine_Start(&ctx));

    CoreEngine_Init(&ctx, 0, 4096);
    CoreEngine_Start(&ctx);

    CHECK_TRUE(IsFlagSet(&ctx, ENGINE_INITIALIZED));
    CHECK_TRUE(IsFlagSet(&ctx, ENGINE_STARTED));
    CHECK_TRUE(!IsFlagSet(&ctx, ENGINE_STOP_REQUESTED));
    CHECK_TRUE(!IsFlagSet(&ctx, ENGINE_AUDIO_THREAD_SILENCED));

    CoreEngine_Stop(&ctx);
    CoreEngine_Deinit(&ctx);
}

TEST(CoreEngine, Stop)
{
    CoreEngineContext ctx;
    
    CHECK_DEATH(CoreEngine_Stop(&ctx));
    CoreEngine_Init(&ctx, 0, 4096);
    CHECK_DEATH(CoreEngine_Stop(&ctx));
    CoreEngine_Start(&ctx);
    CHECK_DEATH(CoreEngine_Deinit(&ctx));

    CoreEngine_Stop(&ctx);

    CHECK_TRUE(IsFlagSet(&ctx, ENGINE_INITIALIZED));
    CHECK_TRUE(!IsFlagSet(&ctx, ENGINE_STARTED));
    CHECK_TRUE(IsFlagSet(&ctx, ENGINE_STOP_REQUESTED));
    CHECK_TRUE(IsFlagSet(&ctx, ENGINE_AUDIO_THREAD_SILENCED));

    CoreEngine_Deinit(&ctx);
}

TEST(CoreEngine, CreateProcessors)
{
    CoreEngineContext ctx;
    FakeProcessor proc[100];
    
    CHECK_DEATH(CoreEngine_CreateProcessor(NULL, NULL, NULL, NULL, NULL));
    CHECK_DEATH(FakeProcessor_Create(&proc[0], &ctx, BUFFER_SIZE));

    CoreEngine_Init(&ctx, 1.0f, 4096);
    for (u16 i = 0; i < 100; i++) {
        FakeProcessor_Create(&proc[i], &ctx, BUFFER_SIZE);
    }

    CoreEngine_Deinit(&ctx);
}

TEST(CoreEngine, ProcessAudio)
{
    CoreEngineContext ctx;
    FakeProcessor proc;
    u16 id;
    f32 silentBuffer[BUFFER_SIZE] = { 0, };

    CHECK_DEATH(CoreEngine_AddSource(NULL, 0));
    CHECK_DEATH(CoreEngine_AddSource(&ctx, 0));

    CoreEngine_Init(&ctx, 1.0f, 4096);
    CHECK_DEATH(CoreEngine_AddSource(&ctx, MAX_PROCESSORS));

    id = FakeProcessor_Create(&proc, &ctx, BUFFER_SIZE);
    CoreEngine_AddSource(&ctx, id);
    CoreEngine_Start(&ctx);

    if (!FakeProcessor_WaitForData(&proc)) {
        CoreEngine_Stop(&ctx);
        CoreEngine_Deinit(&ctx);
        CHECK_TRUE(false);
    }

    CHECK_TRUE(proc.newAudioCycleCalled);
    CHECK_TRUE(proc.numFrames == (BUFFER_SIZE / 2));
    CHECK_TRUE(proc.sampleRate == SAMPLE_RATE_DEFAULT);
    CHECK_TRUE(memcmp(proc.buffer, silentBuffer, proc.numFrames * 2) == 0);

    CoreEngine_Stop(&ctx);
    CoreEngine_Deinit(&ctx);
}

TEST(CoreEngine, Routing)
{
    // TODO
}

TEST_SETUP(CoreEngine)
{
    ADD_TEST(CoreEngine, Init);
    ADD_TEST(CoreEngine, Deinit);
    ADD_TEST(CoreEngine, Start);
    ADD_TEST(CoreEngine, Stop);
    ADD_TEST(CoreEngine, CreateProcessors);
    ADD_TEST(CoreEngine, ProcessAudio);
}

TEST_BRINGUP(CoreEngine)
{

}

TEST_TEARDOWN(CoreEngine)
{
    
}

