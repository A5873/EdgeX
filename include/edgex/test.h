/*
 * EdgeX OS - Test Framework
 *
 * This file provides utilities for unit testing EdgeX OS components.
 */

#ifndef EDGEX_TEST_H
#define EDGEX_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <edgex/ipc.h>
#include <edgex/scheduler.h>
/* Printf function for test compatibility with kernel code */
#define kernel_printf printf

/* Import system headers needed for test implementation */
#include <time.h>
#include <sys/time.h>

/* System time function for timeouts */
static inline uint64_t get_system_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

/*
 * Test implementation registration
 *
 * This section provides a mechanism to register test implementations
 * of system functions. The actual implementations are defined in
 * test_ipc_impl.c
 */

/* Initialize the test environment and register test implementations */
void test_init(void);

/* Mock kernel initialization function */
static inline void kernel_initialize(void) {
    /* Initialize random seed for tests */
    srand((unsigned int)time(NULL));
    
    /* Initialize the test environment */
    test_init();
}
}

/* Test result codes */
#define TEST_PASS      0
#define TEST_FAIL      1
#define TEST_SKIP      2

/* Colors for test output */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* Test case structure */
typedef struct {
    const char* name;
    int (*test_func)(void);
    void (*setup)(void);
    void (*teardown)(void);
} test_case_t;

/* Test suite structure */
typedef struct {
    const char* name;
    test_case_t* cases;
    int case_count;
    void (*suite_setup)(void);
    void (*suite_teardown)(void);
} test_suite_t;

/* Global test statistics */
typedef struct {
    int total;
    int passed;
    int failed;
    int skipped;
} test_stats_t;

extern test_stats_t test_stats;

/* Assertion macros */
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf(ANSI_COLOR_RED "Assertion failed: %s, line %d: " #condition ANSI_COLOR_RESET "\n", \
                   __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_EQUALS(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf(ANSI_COLOR_RED "Assertion failed: %s, line %d: " \
                   "expected " #expected " (%d), got " #actual " (%d)" ANSI_COLOR_RESET "\n", \
                   __FILE__, __LINE__, (int)(expected), (int)(actual)); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_STR_EQUALS(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf(ANSI_COLOR_RED "Assertion failed: %s, line %d: " \
                   "expected \"%s\", got \"%s\"" ANSI_COLOR_RESET "\n", \
                   __FILE__, __LINE__, (expected), (actual)); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            printf(ANSI_COLOR_RED "Assertion failed: %s, line %d: " \
                   "expected NULL, got %p" ANSI_COLOR_RESET "\n", \
                   __FILE__, __LINE__, (ptr)); \
            return TEST_FAIL; \
        } \
    } while (0)

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf(ANSI_COLOR_RED "Assertion failed: %s, line %d: " \
                   "expected non-NULL value" ANSI_COLOR_RESET "\n", \
                   __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while (0)

/* Test fixture macros */
#define TEST_SETUP(suite_name) void suite_name##_setup(void)
#define TEST_TEARDOWN(suite_name) void suite_name##_teardown(void)
#define TEST_CASE(suite_name, test_name) int suite_name##_##test_name(void)

/* Test registration macros */
#define BEGIN_TEST_SUITE(name) \
    test_suite_t name##_suite = { \
        .name = #name, \
        .cases = (test_case_t[]) {

#define TEST(suite_name, test_name) \
            { \
                .name = #test_name, \
                .test_func = suite_name##_##test_name, \
                .setup = NULL, \
                .teardown = NULL \
            },

#define TEST_WITH_FIXTURE(suite_name, test_name) \
            { \
                .name = #test_name, \
                .test_func = suite_name##_##test_name, \
                .setup = suite_name##_setup, \
                .teardown = suite_name##_teardown \
            },

#define END_TEST_SUITE(name) \
        }, \
        .case_count = sizeof(name##_suite.cases) / sizeof(test_case_t), \
        .suite_setup = NULL, \
        .suite_teardown = NULL \
    };

/* Test runner functions */
void run_test_suite(test_suite_t* suite);
void run_all_tests(test_suite_t* suites[], int suite_count);
void print_test_summary(void);

/* Mock kernel initialization function */
static inline void kernel_initialize(void) {
    /* Initialize random seed for tests */
    srand((unsigned int)time(NULL));
}

#endif /* EDGEX_TEST_H */

