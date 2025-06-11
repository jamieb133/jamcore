#include "test_framework.h"
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/_types/_va_list.h>
#include <sys/acl.h>
#include <sys/time.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static jmp_buf jumpBuffer_;

TestSuite testSuites_[MAX_SUITES];
u16 numSuites_ = 0;

typedef struct {
    char* data;
    u64 capacity;
} String;

static inline u64 GetTimeMs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000 + (u64)tv.tv_usec / 1000;
}

void StringInit(String* string, const char* cstring)
{
    string->capacity = (strlen(cstring) + 1) * 2;
    string->data = malloc(string->capacity);
    assert(string->data);
    strcpy(string->data, cstring);
}

void StringDeinit(String* string)
{
    free(string->data);
    string->capacity = 0;
}

void StringConcat(String* string, const char* format, ...)
{
    va_list args;

    va_start(args, format);
    size_t formattedLen = vsnprintf(NULL, 0, format, args);
    va_end(args);

    size_t currentLen = strlen(string->data);
    size_t totalRequiredSize = currentLen + formattedLen + 1;

    if (string->capacity < totalRequiredSize) {
        while (string->capacity < totalRequiredSize) {
            string->capacity *= 2;
        }
        char* newBuffer = realloc(string->data, string->capacity);
        assert(newBuffer);
        string->data = newBuffer;
    }

    va_start(args, format);
    vsnprintf(string->data + currentLen, formattedLen + 1, format, args);
    va_end(args);
}

static void WriteJUnitReport(void)
{
    String junit;
    StringInit(&junit, "<!-- TestResults.xml -->\n");

    StringConcat(&junit, "<testsuites>\n");

    for (u16 i = 0; i < numSuites_; i++) {
        TestSuite* suite = &testSuites_[i]; 
        StringConcat(&junit, "  <testsuite ");
        StringConcat(&junit, "name=\"%s\" ", suite->name);
        StringConcat(&junit, "tests=\"%d\" ", suite->numTests);
        StringConcat(&junit, "failures=\"%d\" ", suite->numTests - suite->numPassed);
        StringConcat(&junit, "skipped=\"%d\" ", 0);
        StringConcat(&junit, "errors=\"%d\" ", 0);
        StringConcat(&junit, "time=\"%d\" ", suite->duration);
        StringConcat(&junit, " >\n");

        for (u16 j = 0; j < suite->numTests; j++) {
            TestInfo* test = &suite->testInfo[j];
            StringConcat(&junit, "    <testcase ");
            StringConcat(&junit, "name=\"%s\" ", test->name);
            StringConcat(&junit, "classname=\"%s\" ", suite->name);
            StringConcat(&junit, "time=\"%d\" ", test->duration);
            if (test->passed) {
                StringConcat(&junit, " />\n");
            }
            else {
                StringConcat(&junit, " >\n");
                StringConcat(&junit, "      <failure message=\"Test Failed\">\n");
                StringConcat(&junit, "        %s:%d\n", test->failureInfo.file, test->failureInfo.line);
                StringConcat(&junit, "        %s\n", test->failureInfo.message);
                StringConcat(&junit, "      </failure>\n");
                StringConcat(&junit, "    </testcase>\n");
            }
        }
        StringConcat(&junit, "  </testsuite>\n");
    }

    StringConcat(&junit, "</testsuites>\n");

    FILE* file = fopen("TestResults.xml", "w");
    assert(file);
    fprintf(file, "%s", junit.data);
    fclose(file);

    StringDeinit(&junit);
}

void TestAssert(bool condition, const char* testMacro, const char* file, i32 line, TestInfo* testInfo) 
{
    if (condition) {
        testInfo->numChecksPassed++;
        return;
    }

    testInfo->failureInfo.message = testMacro;
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
        u64 suiteStartTime = GetTimeMs();
        suite->Setup(suite);

        for (u16 j = 0; j < suite->numTests; j++) {
            TestInfo* test = &suite->testInfo[j];
            u64 testStartTime;
            suite->Bringup(test);

            if (setjmp(jumpBuffer_)) {
                // Test failed
                test->duration = GetTimeMs() - testStartTime;
                failedTests[numFailed] = test;
                test->passed = false;
                numFailed++;
            }
            else {
                // Test passed
                testStartTime = GetTimeMs();
                test->RunTest(test);            
                test->duration = GetTimeMs() - testStartTime;
                test->passed = true;
                suite->numPassed++;
                numPassed++;
            }
            
            suite->Teardown();
        }

        suite->duration = GetTimeMs() - suiteStartTime;
    }

    printf("%s========================================================\n", ANSI_COLOR_BLUE);
    printf("-- Test Summary --\n");
    printf("========================================================%s\n", ANSI_COLOR_RESET);
    
    for (u16 i = 0; i < numSuites_; i++) {
        TestSuite* suite = &testSuites_[i];
        printf("%s: %d passed, %d failed (%d ms)\n", suite->name, suite->numPassed, suite->numTests - suite->numPassed, suite->duration);
    }

    if (numFailed > 0) {
        printf("%sTests failed! (%d passed, %d failed)%s\n", ANSI_COLOR_RED, numPassed, numFailed, ANSI_COLOR_RESET);
        for (u16 i = 0; i < numFailed; i++) {
            TestInfo* test = failedTests[i];
            printf("%s -- %s (%s:%d)%s\n", ANSI_COLOR_YELLOW, test->name, test->failureInfo.file, test->failureInfo.line, ANSI_COLOR_RESET);
        }
    }
    else {
        printf("%sAll tests passed! (%d passed, %d failed)%s\n", ANSI_COLOR_GREEN, numPassed, numFailed, ANSI_COLOR_RESET);
    }

    WriteJUnitReport();

    return numFailed > 0 ? 1 : 0;
}

