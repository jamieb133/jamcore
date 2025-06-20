#include "logger.h"
#include "test_framework.h"

INCLUDE_TEST_SUITE(CoreEngine)
INCLUDE_TEST_SUITE(Oscillators)
INCLUDE_TEST_SUITE(ThreadPool)

int main()
{
    ADD_TEST_SUITE(ThreadPool);
    ADD_TEST_SUITE(CoreEngine);
    ADD_TEST_SUITE(Oscillators);

    return RunAllTests(LOG_TEST);
}

