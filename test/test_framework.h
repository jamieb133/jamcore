#pragma once

#include <types.h>
#include <stdbool.h>
#include <stdlib.h>
#include <setjmp.h>
#include <pthread.h>

#include <logger.h>

#define MAX_TESTS_PER_SUITE 128
#define MAX_SUITES 64

typedef struct TestInfo {
    const char* name;
    u64 numChecksPassed;
    u32 duration;
    bool passed;
    bool expectAssert;
    struct {
        i32 line;
        const char* file;
        char* message;
    } failureInfo;
    void (*RunTest)(struct TestInfo*);
} TestInfo;

typedef struct TestSuite{
    const char* name;
    u8 numTests;
    u8 numFailed;
    u32 duration;
    TestInfo testInfo[MAX_TESTS_PER_SUITE];
    void (*Setup)(struct TestSuite*);
    void (*Bringup)(struct TestInfo*);
void (*Teardown)(void);
} TestSuite;

extern TestSuite testSuites_[MAX_SUITES];
extern u16 numSuites_;
extern jmp_buf testFailedJumpBuffer_;
extern jmp_buf assertJumpBuffer_;

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

#define _TEST_SETUP_FUNC(suiteName) __##suiteName##_##Setup
#define _TEST_BRINGUP_FUNC(suiteName) __##suiteName##_##Bringup
#define _TEST_TEARDOWN_FUNC(suiteName) __##suiteName##_##Teardown
#define _TEST_BODY_FUNC(testSuite, testName) __##testSuite##_##testName

#define TEST_SETUP(suiteName) void _TEST_SETUP_FUNC(suiteName) (struct TestSuite* __testSuite)
#define TEST_BRINGUP(suiteName) void _TEST_BRINGUP_FUNC(suiteName) (struct TestInfo* __testInfo)
#define TEST_TEARDOWN(suiteName) void _TEST_TEARDOWN_FUNC(suiteName) (void)
#define TEST(testSuite, testName) void _TEST_BODY_FUNC(testSuite, testName) (struct TestInfo* __testInfo)

#define INCLUDE_TEST_SUITE(suiteName) extern void _TEST_SETUP_FUNC(suiteName) (struct TestSuite* s);\
                                      extern void _TEST_BRINGUP_FUNC(suiteName) (struct TestInfo* i);\
                                      extern void _TEST_TEARDOWN_FUNC(suiteName) (void);

#define ADD_TEST_SUITE(suiteName) testSuites_[numSuites_++] = (TestSuite) { .name = #suiteName, .numTests = 0, .numFailed = 0, \
                                                                          .Setup = _TEST_SETUP_FUNC(suiteName), \
                                                                          .Bringup = _TEST_BRINGUP_FUNC(suiteName), \
                                                                          .Teardown = _TEST_TEARDOWN_FUNC(suiteName) }

#define ADD_TEST(testSuite, testName) __testSuite->testInfo[__testSuite->numTests++] = (TestInfo) { .name = #testName,\
                                                                                                    .numChecksPassed = 0, \
                                                                                                    .expectAssert = false,\
                                                                                                    .failureInfo.message = NULL,\
                                                                                                    .RunTest = _TEST_BODY_FUNC(testSuite, testName) };

#define CHECK_DEATH(function)\
{\
    __testInfo->expectAssert = true;\
    if (setjmp(assertJumpBuffer_) == 0) {\
        (function);\
        __testInfo->expectAssert = false;\
        TestCheck(false, "CHECK_DEATH(" TO_STRING(function) ")", __FILE__, __LINE__, __testInfo);\
    }\
    else {\
        __testInfo->numChecksPassed++;\
        __testInfo->expectAssert = false;\
    }\
}

#define CHECK_TRUE(cond) TestCheck(cond, "CHECK_TRUE(" TO_STRING(cond) ")", __FILE__, __LINE__, __testInfo);

void TestCheck(bool condition, const char* message, const char* file, i32 line, TestInfo* testResult);

i32 RunAllTests(LogLevel logLevel);

