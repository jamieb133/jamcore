#include "test_framework.h"
#include <logger.h>

#include <stdio.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/_pthread/_pthread_cond_t.h>
#include <sys/_pthread/_pthread_mutex_t.h>
#include <sys/_types/_va_list.h>
#include <sys/acl.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef struct {
    char* data;
    u64 capacity;
} String;

TestSuite testSuites_[MAX_SUITES];
u16 numSuites_ = 0;
u16 currentTestNum_ = 0;
pthread_t threadPool_[NUM_THREADS];
pthread_mutex_t testRunnerMutex_;

// Thread local globals to allow accessing current test and fail a test from Assert
__thread TestInfo* currentTest_;
__thread jmp_buf testFailedJumpBuffer_;
__thread jmp_buf assertJumpBuffer_;

static void StringInit(String* string, const char* cstring)
{
    string->capacity = (strlen(cstring) + 1) * 2;
    string->data = malloc(string->capacity);
    assert(string->data);
    strcpy(string->data, cstring);
}

static void StringDeinit(String* string)
{
    free(string->data);
    string->capacity = 0;
}

static void StringConcat(String* string, const char* format, ...)
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
        StringConcat(&junit, "failures=\"%d\" ", suite->numFailed);
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

static void AssertHandler(const char* message, const char* file, i32 line)
{
    if (currentTest_->expectAssert) {
        longjmp(assertJumpBuffer_, 1);
    }
    else {
        // Unexpected assert, fail test now
        TestCheck(false, message, file, line, currentTest_);
    }
}

void TestCheck(bool condition, const char* msg, const char* file, i32 line, TestInfo* testInfo) 
{
    if (condition) {
        testInfo->numChecksPassed++;
        return;
    }

    testInfo->failureInfo.file = file;
    testInfo->failureInfo.line = line;
    testInfo->failureInfo.message = malloc(strlen(msg) + 1);
    memset(testInfo->failureInfo.message, 0, strlen(msg) + 1);
    strcpy(testInfo->failureInfo.message, msg);
    longjmp(testFailedJumpBuffer_, 1);
}

void* TestRunner(void* args)
{
    for (;;) {
        pthread_mutex_lock(&testRunnerMutex_);

        if (currentTestNum_ >= numSuites_) {
            pthread_mutex_unlock(&testRunnerMutex_);
            break;
        }

        TestSuite* suite = &testSuites_[currentTestNum_];
        currentTestNum_++;
        pthread_mutex_unlock(&testRunnerMutex_);

        u64 suiteStartTime = GetTimeMs();
        suite->Setup(suite);

        for (u16 j = 0; j < suite->numTests; j++) {
            u64 testStartTime;
            currentTest_ = &suite->testInfo[j];
            suite->Bringup(currentTest_);

            if (setjmp(testFailedJumpBuffer_)) {
                // Test failed
                currentTest_->duration = GetTimeMs() - testStartTime;
                currentTest_->passed = false;
                suite->numFailed++;
            }
            else {
                // Test passed
                testStartTime = GetTimeMs();
                currentTest_->RunTest(currentTest_);
                currentTest_->duration = GetTimeMs() - testStartTime;
                currentTest_->passed = true;
            }
            suite->Teardown();
        }
        suite->duration = GetTimeMs() - suiteStartTime;
    }

    return NULL;
}

i32 RunAllTests(LogLevel logLevel)
{
    RegisterAssertHandler(AssertHandler);
    SetLogLevel(logLevel);

    pthread_mutex_init(&testRunnerMutex_, NULL);

    for (u8 i = 0; i < NUM_THREADS; i++) {
        pthread_t threadId;
        pthread_create(&threadId, NULL, TestRunner, NULL);
        threadPool_[i] = threadId;
    }

    for (u8 i = 0; i < NUM_THREADS; i++) {
        pthread_join(threadPool_[i], NULL);
    }
    
    printf("%s========================================================\n", ANSI_COLOR_BLUE);
    printf("-- Test Summary --\n");
    printf("========================================================%s\n", ANSI_COLOR_RESET);
    
    u16 totalPassed = 0, totalFailed = 0;
    for (u16 i = 0; i < numSuites_; i++) {
        TestSuite* suite = &testSuites_[i];
        totalPassed += suite->numTests - suite->numFailed;
        totalFailed += suite->numFailed;
    }

    if (totalFailed > 0) {
        printf("%sTests failed! (%d passed, %d failed)%s\n", ANSI_COLOR_RED, totalPassed, totalFailed, ANSI_COLOR_RESET);
        for (u16 i = 0; i < numSuites_; i++) {
            TestSuite* suite = &testSuites_[i];
            if (suite->numFailed == 0) {
                continue;
            }
            printf("  > %s (%d):\n", suite->name, suite->numFailed);
            for (u16 j = 0; j < suite->numTests; j++) {
                TestInfo* test = &suite->testInfo[j];
                if (test->passed) {
                    continue;
                }
                
                printf("     -- %s (%s:%d)\n", 
                        test->name, 
                        test->failureInfo.file, 
                        test->failureInfo.line);
                printf("        %s%s%s\n", ANSI_COLOR_YELLOW, test->failureInfo.message, ANSI_COLOR_RESET);
                free(test->failureInfo.message);
            }
        }
    }
    else {
        printf("%sAll tests passed! (%d passed, %d failed)%s\n", 
                ANSI_COLOR_GREEN, totalPassed, totalFailed, ANSI_COLOR_RESET);
    }

    WriteJUnitReport();
    pthread_mutex_destroy(&testRunnerMutex_);

    return totalFailed > 0 ? 1 : 0;
}

