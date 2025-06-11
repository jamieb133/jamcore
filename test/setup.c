#include "test_framework.h"

INCLUDE_TEST_SUITE(ExampleTests)

int main()
{
    ADD_TEST_SUITE(ExampleTests);

    return RunAllTests();
}

