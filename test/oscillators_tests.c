#include "logger.h"
#include "oscillators.h"
#include "test_framework.h"

TEST(Oscillators, SinWave)
{
    ASSERT_TRUE(false);
}

TEST(Oscillators, SawWave)
{
    ASSERT_TRUE(false);
}

TEST(Oscillators, SquareWave)
{
    ASSERT_TRUE(true);
}

TEST_SETUP(Oscillators)
{
    ADD_TEST(Oscillators, SinWave);
    ADD_TEST(Oscillators, SawWave);
    ADD_TEST(Oscillators, SquareWave);
}

TEST_BRINGUP(Oscillators)
{

}

TEST_TEARDOWN(Oscillators)
{

}

