#ifdef ENABLE_TESTS

#ifndef LISP_TEST_H
#define LISP_TEST_H

#include "common.h"

// Sprint 0 means that it is a general unit test.
// All other sprint numbers are associated with the sprint of that number.

#define DEFINE_UNIT_TEST(func, sprint) \
    { #func, sprint, true, { .unit_test = func } }

#define DEFINE_INTEGRATION_TEST(name, sprint, input_arg, output_arg) \
    { name, sprint, false, { .input = input_arg, .output = output_arg } }

typedef union {
    bool (*unit_test)();
    struct {
        uint8_t* input;
        size_t input_len;
        uint8_t* output;
        size_t output_len;
    };
} TestUnion;

typedef struct {
    const char* name;
    size_t sprint;

    bool is_unit_test;
    TestUnion test;
} TestDefinition;

typedef struct {
    TestDefinition* tests;
    size_t test_count;
} TestList;

#endif
#endif
