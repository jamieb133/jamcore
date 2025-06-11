#include "test_framework.h"

INCLUDE_TEST_SUITE(CoreEngine)
INCLUDE_TEST_SUITE(Oscillators)
INCLUDE_TEST_SUITE(ExampleTests)

int main()
{
    ADD_TEST_SUITE(CoreEngine);
    ADD_TEST_SUITE(Oscillators);
    ADD_TEST_SUITE(ExampleTests);

    return RunAllTests();
}

