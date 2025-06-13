#include "test_framework.h"
#include <core_engine.h>

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

TEST(CoreEngine, Routing)
{

}

TEST_SETUP(CoreEngine)
{
    ADD_TEST(CoreEngine, Init);
    ADD_TEST(CoreEngine, Deinit);
    ADD_TEST(CoreEngine, Start);
    ADD_TEST(CoreEngine, Stop);
}

TEST_BRINGUP(CoreEngine)
{
    printf("%s\n", __testInfo->name);
}

TEST_TEARDOWN(CoreEngine)
{
    
}

