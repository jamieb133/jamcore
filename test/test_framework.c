#include "test_framework.h"
#include <stdio.h>
#include <setjmp.h>
#include <string.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static jmp_buf jumpBuffer_;

TestSuite testSuites_[MAX_SUITES];
u16 numSuites_ = 0;

void TestAssert(bool condition, const char* file, i32 line, TestInfo* testInfo) 
{
    if (condition) {
        testInfo->numChecksPassed++;
        return;
    }

    testInfo->failureInfo.file = file;
    testInfo->failureInfo.line = line;
    longjmp(jumpBuffer_, 1);
}

i32 RunAllTests()
{
    u16 numPassed = 0;
    u16 numFailed = 0;
    TestInfo* failedTests[MAX_SUITES * MAX_TESTS_PER_SUITE];

    for (u16 i = 0; i < numSuites_; i++) {
        TestSuite* suite = &testSuites_[i];
        suite->Setup(suite);

        printf("%s========================================================\n", ANSI_COLOR_YELLOW);
        printf("-- %s (%d Tests) --\n", suite->name, suite->numTests);
        printf("========================================================%s\n", ANSI_COLOR_RESET);

        for (u16 j = 0; j < suite->numTests; j++) {
            TestInfo* test = &suite->testInfo[j];
            suite->Bringup(test);

            if (setjmp(jumpBuffer_)) {
                failedTests[numFailed] = test;
                numFailed++;
            }
            else {
                test->RunTest(test);            
                suite->numPassed++;
                numPassed++;
            }
            
            suite->Teardown();
        }
    }

    printf("%sTests Finished (%d ran %d failed)%s\n", numFailed ? ANSI_COLOR_RED : ANSI_COLOR_GREEN, numPassed+numFailed, numFailed, ANSI_COLOR_RESET);

    if (numFailed == 0) {
        return 0;
    }
    
    printf("\n");
    for (u16 i = 0; i < numFailed; i++) {
        TestInfo* test = failedTests[i];
        printf(" -- %s%s (%s:%d)%s\n", ANSI_COLOR_YELLOW, test->name, test->failureInfo.file, test->failureInfo.line, ANSI_COLOR_RESET);
    }

    return 1;
}

