#ifdef ENABLE_TESTS

#ifndef LISP_TEST_H
#define LISP_TEST_H

#include "common.h"

// Sprint 0 means that it is a general unit test.
// All other sprint numbers are associated with the sprint of that number.
#define DEFINE_TEST(func, sprint) \
    ((TestDefinition) { #func, sprint, func })

typedef struct {
    const char* name;
    size_t sprint;

    bool (*test)();
} TestDefinition;

typedef struct {
    TestDefinition* tests;
    size_t test_count;
} TestList;

#endif
#endif
