#ifndef LISP_UTIL_H
#define LISP_UTIL_H

#include <stdio.h>
#include <stdlib.h>

/// Returns the number of objects in the array.
#define countof(obj) ((size_t) (sizeof(obj) / sizeof(*(obj))))
/// Returns the alignment of the given type.
#define alignof(type) offsetof(struct { uint8_t c; type d; }, d)

#define ASSERT(assertion, ...) do { \
    if ((assertion)) break; \
    PANIC_INTERNAL("assertion failed: " #assertion "\n" __VA_ARGS__); \
} while (0)

#define UNREACHABLE(...) \
    PANIC_INTERNAL("internal error: reached unreachable code\n" __VA_ARGS__)

#define PANIC(initial_message, ...) 

#define PANIC_INTERNAL(...) do { \
    fprintf( \
        stderr, \
        "program exited at %s:%u in %s\n", \
        __FILE__, \
        __LINE__, \
        __func__ \
    ); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    abort(); \
} while (0)

#endif
