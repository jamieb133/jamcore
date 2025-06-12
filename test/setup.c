#include "test_framework.h"

INCLUDE_TEST_SUITE(CoreEngine)
INCLUDE_TEST_SUITE(Oscillators)

int main()
{
    ADD_TEST_SUITE(CoreEngine);
    //ADD_TEST_SUITE(Oscillators);

    return RunAllTests();
}

