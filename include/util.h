#ifndef LISP_UTIL_H
#define LISP_UTIL_H

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

/// Returns the number of objects in the array.
#define countof(obj) ((size_t) (sizeof(obj) / sizeof(*(obj))))
/// Returns the alignment of the given type.
#define alignof(type) offsetof(struct { uint8_t c; type d; }, d)

static inline bool valid_capacity(size_t capacity, size_t size) {
    return capacity <= PTRDIFF_MAX / size;
}

static inline bool grow_capacity(size_t* capacity, size_t size, size_t base_capacity) {
    size_t new_capacity = *capacity == 0 ? base_capacity : *capacity * 2;
    if (new_capacity < *capacity || !valid_capacity(new_capacity, size)) {
        new_capacity = PTRDIFF_MAX / size;
    }

    if (new_capacity <= *capacity) {
        return false;
    }

    *capacity = new_capacity;
    return true;
}

static inline bool grow(void** ptr, size_t* capacity, size_t size, size_t base_capacity) {
    size_t tmp_capacity = *capacity;
    if (!grow_capacity(&tmp_capacity, size, base_capacity)) return false;

    size_t alloc_size = tmp_capacity * size;
    void* new_ptr = realloc(*ptr, alloc_size);
    if (new_ptr == NULL) {
        return false;
    }

    *capacity = tmp_capacity;
    *ptr = new_ptr;
    return true;
}

static inline bool is_power_of_two(uintmax_t val) {
    if (val == 0) return true;

    uintmax_t val_0 = val & (val - 1);
    if (val_0 == 0) return true;

    return false;
}

static inline uintmax_t floor_power_of_two(uintmax_t val) {
    while ((val & (val - 1)) != 0) {
        val &= (val - 1);
    }

    return val;
}

#define GROW(ptr, capacity, size, base_capacity) \
    grow((void**) (ptr), (capacity), (size), (base_capacity))

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
