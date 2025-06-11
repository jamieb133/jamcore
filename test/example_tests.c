#include "test_framework.h"
#include <stdio.h>
#include <unistd.h>

TEST(ExampleTests, MyFailingTest)
{
    ASSERT_TRUE(true);
    ASSERT_TRUE(true);
    ASSERT_TRUE(true);
    ASSERT_TRUE(false); // <-- Should fail here!
    ASSERT_TRUE(true);
    ASSERT_TRUE(true);
}

TEST(ExampleTests, MyPassingTest)
{
    usleep(1234 * 1000); // Demonstrate the duration output 
    for (u8 i = 0; i < 128; i++) {
        ASSERT_TRUE(i < 128);
    }
}

TEST_SETUP(ExampleTests)
{
    ADD_TEST(ExampleTests, MyFailingTest);
    ADD_TEST(ExampleTests, MyPassingTest);
}

TEST_BRINGUP(ExampleTests)
{
    printf("Starting %s test...\n", __testInfo->name);
}

TEST_TEARDOWN(ExampleTests)
{

}

