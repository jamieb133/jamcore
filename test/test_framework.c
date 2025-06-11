#include "test_framework.h"
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

#define NUM_THREADS 4

static jmp_buf jumpBuffer_;
static u16 currentTestNum_ = 0;
static pthread_t threadPool_[NUM_THREADS];
static pthread_mutex_t mutex_;

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

void* TestRunner(void* args)
{
    (void)args;
    
    for (;;) {
        pthread_mutex_lock(&mutex_);

        if (currentTestNum_ >= numSuites_) {
            pthread_mutex_unlock(&mutex_);
            break;
        }

        TestSuite* suite = &testSuites_[currentTestNum_];
        currentTestNum_++;
        pthread_mutex_unlock(&mutex_);

        u64 suiteStartTime = GetTimeMs();
        suite->Setup(suite);

        for (u16 j = 0; j < suite->numTests; j++) {
            TestInfo* test = &suite->testInfo[j];
            u64 testStartTime;
            printf("%s %s, %d\n", suite->name, test->name, suite->numTests);
            suite->Bringup(test);

            if (setjmp(jumpBuffer_)) {
                // Test failed
                test->duration = GetTimeMs() - testStartTime;
                test->passed = false;
                suite->numFailed++;
            }
            else {
                // Test passed
                testStartTime = GetTimeMs();
                test->RunTest(test);            
                test->duration = GetTimeMs() - testStartTime;
                test->passed = true;
            }
            
            suite->Teardown();
        }

        suite->duration = GetTimeMs() - suiteStartTime;
    }

    return NULL;
}

i32 RunAllTests()
{
    pthread_mutex_init(&mutex_, NULL);

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
            printf("%s > %s (%d):%s\n", ANSI_COLOR_YELLOW, suite->name, suite->numFailed, ANSI_COLOR_RESET);
            for (u16 j = 0; j < suite->numTests; j++) {
                TestInfo* test = &suite->testInfo[j];
                if (test->passed) {
                    continue;
                }
                
                printf("%s   -- %s (%s:%d)%s\n", 
                        ANSI_COLOR_YELLOW, 
                        test->name, 
                        test->failureInfo.file, 
                        test->failureInfo.line, 
                        ANSI_COLOR_RESET);
            }
        }
    }
    else {
        printf("%sAll tests passed! (%d passed, %d failed)%s\n", 
                ANSI_COLOR_GREEN, totalPassed, totalFailed, ANSI_COLOR_RESET);
    }

    WriteJUnitReport();
    pthread_mutex_destroy(&mutex_);

    return totalFailed > 0 ? 1 : 0;
}

